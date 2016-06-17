/*
 * Copyright 2016 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <net/ethernet.h>
#include <net/if_arp.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <vnet/vnet.h>
#include <vnet/plugin/plugin.h>
#include <vnet/ip/ip.h>
#include <vnet/unix/tuntap.h>
#include <librtnl/mapper.h>
#include <vnet/ethernet/arp_packet.h>

enum {
	NEXT_ARP = 0,
	NEXT_ICMP = 0,
	NEXT_INJECT_ARP_ICMP,
};

enum {
	NEXT_DROP,
	NEXT_INJECT_CLASSIFIED,
};

enum {
	ERROR_DROP = 0,
	ERROR_INJECT_ARP,
	ERROR_INJECT_ICMP,
	ERROR_INJECT_CLASSIFIED,
};

static char *error_strings[] = {
	[ERROR_DROP] = "Untapped interface",
	[ERROR_INJECT_ARP] = "Inject ARP",
	[ERROR_INJECT_ICMP] = "Inject ICMP",
	[ERROR_INJECT_CLASSIFIED] = "Inject Classified",
};

vlib_node_registration_t tap_inject_arp_node;
vlib_node_registration_t tap_inject_icmp_node;
vlib_node_registration_t tap_inject_classified_node;

struct tap_to_iface {
	u32 tap;
	u32 iface;
};

struct router_main {
	vnet_main_t *vnet_main;
	u32 *iface_to_tap;
	struct tap_to_iface *tap_to_iface;
	u32 ns_index;
};

static struct router_main rm;

static int
set_tap_hwaddr(vlib_main_t *m, char *name, u8 *hwaddr)
{
	int fd, rc;
	struct ifreq ifr;

	fd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (fd < 0)
		return -1;

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, (char *)name, sizeof(ifr.ifr_name) - 1);
	memcpy(ifr.ifr_hwaddr.sa_data, hwaddr, ETHER_ADDR_LEN);
	ifr.ifr_hwaddr.sa_family = ARPHRD_ETHER;
	rc = ioctl(fd, SIOCSIFHWADDR, &ifr) < 0 ? -1 : 0;
	close(fd);
	return rc;
}

static clib_error_t *
do_tap_connect(vlib_main_t *m, char *name, u32 iface, u32 *tap)
{
	vnet_hw_interface_t *hw = vnet_get_hw_interface(rm.vnet_main, iface);

	*tap = ~0;
	if (!hw)
		return clib_error_return(0, "invalid interface");

	if (vnet_tap_connect(m, (u8 *)name, hw->hw_address, tap))
		return clib_error_return(0, "failed to connect tap");

	if (set_tap_hwaddr(m, name, hw->hw_address))
		return clib_error_return(0, "failed to set tap hw address");

	if (set_int_l2_mode(m, rm.vnet_main, MODE_L2_XC, *tap, 0, 0, 0, iface))
		return clib_error_return(0, "failed to xconnect to interface");

	return vnet_sw_interface_set_flags(rm.vnet_main, *tap,
	                                   VNET_SW_INTERFACE_FLAG_ADMIN_UP);
}

enum {
	PROTO_ARP = 0,
	PROTO_ICMP4,
	PROTO_OSPF2,
	PROTO_N_TOTAL,
};

enum {
	PROTO_BIT_ARP = 1 << PROTO_ARP,
	PROTO_BIT_ICMP4 = 1 << PROTO_ICMP4,
	PROTO_BIT_OSPF2 = 1 << PROTO_OSPF2,
};

static char *proto_strings[PROTO_N_TOTAL] = {
	[PROTO_ARP] = "arp",
	[PROTO_ICMP4] = "icmp4",
	[PROTO_OSPF2] = "ospf2",
};

static inline u32 parse_protos(char *proto_string)
{
	u32 protos = 0;
	char *tok, **proto;

	for (tok = strtok(proto_string, ","); tok; tok = strtok(NULL, ","))
		for (proto = proto_strings; proto && *proto; ++proto)
			if (!strncmp(tok, *proto, 16))
				protos |= 1 << (proto - proto_strings);
	return protos;
}

static uword unformat_protos(unformat_input_t *input, va_list *args)
{
	u32 *protos = va_arg(*args, u32 *);
	u8 *proto_string;

	if (unformat(input, "%s", &proto_string))
		*protos = parse_protos((char *)proto_string);
	return 1;
}

static void add_del_addr(ns_addr_t *a, int is_del)
{
	struct tap_to_iface *map = NULL;
	u32 sw_if_index = ~0;

	vec_foreach(map, rm.tap_to_iface) {
		if (a->ifaddr.ifa_index == map->tap) {
			sw_if_index = map->iface;
			break;
		}
	}

	if (sw_if_index == ~0)
		return;

	ip4_add_del_interface_address(vlib_get_main(),
			sw_if_index, (ip4_address_t *)a->local,
			a->ifaddr.ifa_prefixlen, is_del);
}

static void add_del_route(ns_route_t *r, int is_del)
{
	struct tap_to_iface *map = NULL;
	u32 sw_if_index = ~0;

	vec_foreach(map, rm.tap_to_iface) {
		if (r->oif == map->tap) {
			sw_if_index = map->iface;
			break;
		}
	}

	if (sw_if_index == ~0 || r->table != 254)
		return;

	ip4_add_del_route_next_hop(&ip4_main,
			is_del ? IP4_ROUTE_FLAG_DEL : IP4_ROUTE_FLAG_ADD,
	                (ip4_address_t *)r->dst, r->rtm.rtm_dst_len,
	                (ip4_address_t *)r->gateway, sw_if_index, 0, ~0, 0);
}

static void
netns_notify_cb(void *obj, netns_type_t type, u32 flags, uword opaque)
{
	if (type == NETNS_TYPE_ADDR)
		add_del_addr((ns_addr_t *)obj, flags & NETNS_F_DEL);
	else if (type == NETNS_TYPE_ROUTE)
		add_del_route((ns_route_t *)obj, flags & NETNS_F_DEL);
}

static void insert_tap_to_iface(u32 tap, u32 iface)
{
	struct tap_to_iface map = {
		.tap = tap,
		.iface = iface,
	};

	vec_add1(rm.tap_to_iface, map);
}

static clib_error_t *
tap_inject(vlib_main_t *m, unformat_input_t *input, vlib_cli_command_t *cmd)
{
	char *name = NULL;
	u32 iface = ~0, tap = ~0, protos = 0;
	clib_error_t *err;

	while (unformat_check_input(input) != UNFORMAT_END_OF_INPUT) {
		if (unformat(input, "from %U", unformat_vnet_sw_interface,
		             rm.vnet_main, &iface))
			;
		else if (unformat(input, "as %s", &name))
			;
		else if (unformat(input, "%U", unformat_protos, &protos))
			;
		else
			break;
	}

	if (!protos)
		return clib_error_return(0,
				"no protocols specified");
	else if (iface == ~0)
		return clib_error_return(0,
				"interface name is missing or invalid");
	else if (!name)
		return clib_error_return(0,
				"host interface name is missing or invalid");

	if (protos & PROTO_BIT_OSPF2) /* Require arp and icmp4 for ospf2. */
		if (!(protos & PROTO_BIT_ARP) || !(protos & PROTO_BIT_ICMP4))
			return clib_error_return(0,
				"ospf2 requires arp and icmp4");

	err = do_tap_connect(m, name, iface, &tap);
	if (err) {
		if (tap != ~0)
			vnet_tap_delete(m, tap);
		return err;
	}

	if ((protos & PROTO_BIT_ARP) || (protos & PROTO_BIT_ICMP4)) {
		if (rm.ns_index == ~0) {
			char nsname = 0;
			netns_sub_t sub = {
				.notify = netns_notify_cb,
				.opaque = 0,
			};

			rm.ns_index = netns_open(&nsname, &sub);
			if (rm.ns_index == ~0) {
				vnet_tap_delete(m, tap);
				clib_error_return(0,
					"failed to open namespace");
			}
		}
	}

	if (protos & PROTO_BIT_ARP)
		ethernet_register_input_type(m, ETHERNET_TYPE_ARP,
		                             tap_inject_arp_node.index);

	if (protos & PROTO_BIT_ICMP4)
		ip4_register_protocol(IP_PROTOCOL_ICMP,
		                      tap_inject_icmp_node.index);

	if (protos & PROTO_BIT_OSPF2)
		ip4_register_protocol(IP_PROTOCOL_OSPF,
		                      tap_inject_icmp_node.index);

	/* Find sw_if_index of tap associated with data plane interface. */
	rm.iface_to_tap[iface] = tap;

	/* Find data plane interface associated with host tap ifindex. */
	insert_tap_to_iface(if_nametoindex(name), iface);

	return 0;
}

VLIB_CLI_COMMAND(tap_inject_command, static) = {
	.path = "tap inject",
	.short_help = "tap inject <protocol[,protocol...]> from <intfc-name> as <host-intfc-name>",
	.function = tap_inject,
};

static uword
tap_inject_arp_icmp(vlib_main_t *m, vlib_node_runtime_t *node,
                    vlib_frame_t *f, int arp)
{
	u32 n_left_from = f->n_vectors;
	u32 *from = vlib_frame_vector_args(f);
	u32 next_index = node->cached_next_index;
	u32 *to_next;
	u32 me = arp ? tap_inject_arp_node.index : tap_inject_icmp_node.index;

	while (n_left_from) {
		vlib_buffer_t *b0;
		u32 next0;
		u32 bi0;
		u32 n_left_to_next;
		u32 vlib_rx, vlib_tx;
		u32 counter;

		vlib_get_next_frame(m, node, next_index, to_next,
		                    n_left_to_next);

		*(to_next++) = bi0 = *(from++);
		--n_left_from;
		--n_left_to_next;

		b0 = vlib_get_buffer(m, bi0);

		/* FIXME: What about VLAN? */
		b0->current_data -= sizeof(ethernet_header_t);
		b0->current_length += sizeof(ethernet_header_t);

		vlib_rx = vnet_buffer(b0)->sw_if_index[VLIB_RX];
		vlib_tx = rm.iface_to_tap[vlib_rx];
		vnet_buffer(b0)->sw_if_index[VLIB_TX] = vlib_tx;

		if (vlib_tx == 0 || vlib_tx == ~0)
			next0 = arp ? NEXT_ARP : NEXT_ICMP;
		else {
			next0 = NEXT_INJECT_ARP_ICMP;
			counter = arp ? ERROR_INJECT_ARP : ERROR_INJECT_ICMP;
			vlib_node_increment_counter(m, me, counter, 1);
		}

		vlib_validate_buffer_enqueue_x1(m, node, next_index, to_next,
		                                n_left_to_next, bi0, next0);
		vlib_put_next_frame(m, node, next_index, n_left_to_next);
	}
	return f->n_vectors;
}

static uword
tap_inject_icmp(vlib_main_t *m, vlib_node_runtime_t *node, vlib_frame_t *f)
{
	return tap_inject_arp_icmp(m, node, f, 0 /* ICMP */);
}

VLIB_REGISTER_NODE(tap_inject_icmp_node) = {
	.function = tap_inject_icmp,
	.name = "tap-inject-icmp",
	.vector_size = sizeof(u32),
	.type = VLIB_NODE_TYPE_INTERNAL,
	.n_errors = ARRAY_LEN(error_strings),
	.error_strings = error_strings,
	.n_next_nodes = 2,
	.next_nodes = {
		[NEXT_ICMP] = "ip4-icmp-input",
		[NEXT_INJECT_ARP_ICMP] = "interface-output",
	},
};

static uword
tap_inject_classified(vlib_main_t *m, vlib_node_runtime_t *node,
                      vlib_frame_t *f)
{
	u32 n_left_from = f->n_vectors;
	u32 *from = vlib_frame_vector_args(f);
	u32 next_index = node->cached_next_index;
	u32 *to_next;
	u32 out = 0, drop = 0;

	while (n_left_from) {
		vlib_buffer_t *b0;
		u32 next0;
		u32 bi0;
		u32 n_left_to_next;
		u32 vlib_rx, vlib_tx;

		vlib_get_next_frame(m, node, next_index, to_next,
		                    n_left_to_next);

		*(to_next++) = bi0 = *(from++);
		--n_left_from;
		--n_left_to_next;

		b0 = vlib_get_buffer(m, bi0);

		/* FIXME: What about VLAN? */
		b0->current_data -= sizeof(ethernet_header_t);
		b0->current_length += sizeof(ethernet_header_t);

		vlib_rx = vnet_buffer(b0)->sw_if_index[VLIB_RX];
		vlib_tx = rm.iface_to_tap[vlib_rx];
		vnet_buffer(b0)->sw_if_index[VLIB_TX] = vlib_tx;

		if (vlib_tx == 0 || vlib_tx == ~0) {
			next0 = NEXT_DROP;
			++drop;
		} else {
			next0 = NEXT_INJECT_CLASSIFIED;
			++out;
		}
		vlib_validate_buffer_enqueue_x1(m, node, next_index, to_next,
		                                n_left_to_next, bi0, next0);
		vlib_put_next_frame(m, node, next_index, n_left_to_next);
	}
	vlib_node_increment_counter(m, tap_inject_classified_node.index,
	                            ERROR_DROP, drop);
	vlib_node_increment_counter(m, tap_inject_classified_node.index,
	                            ERROR_INJECT_CLASSIFIED, out);
	return f->n_vectors;
}

VLIB_REGISTER_NODE(tap_inject_classified_node) = {
	.function = tap_inject_classified,
	.name = "tap-inject-classified",
	.vector_size = sizeof(u32),
	.type = VLIB_NODE_TYPE_INTERNAL,
	.n_errors = ARRAY_LEN(error_strings),
	.error_strings = error_strings,
	.n_next_nodes = 2,
	.next_nodes = {
		[NEXT_DROP] = "error-drop",
		[NEXT_INJECT_CLASSIFIED] = "interface-output",
	},
};

static clib_error_t *
interface_add_del(struct vnet_main_t *m, u32 hw_if_index, u32 add)
{
	vnet_hw_interface_t *hw = vnet_get_hw_interface(m, hw_if_index);
	vnet_sw_interface_t *sw = vnet_get_sw_interface(m, hw->sw_if_index);
	ASSERT(hw->sw_if_index == sw->sw_if_index);

	vec_validate(rm.iface_to_tap, sw->sw_if_index);
	rm.iface_to_tap[sw->sw_if_index] = ~0;
	return 0;
}
VNET_HW_INTERFACE_ADD_DEL_FUNCTION(interface_add_del);

clib_error_t *
vlib_plugin_register(vlib_main_t *m, vnet_plugin_handoff_t *h, int f)
{
	rm.vnet_main = h->vnet_main;
	rm.ns_index = ~0;
	return 0;
}

static clib_error_t *router_init(vlib_main_t *m)
{
	return 0;
}
VLIB_INIT_FUNCTION(router_init);

static inline void
update_arp_entry(vlib_buffer_t *b0, ethernet_arp_header_t *arp, u32 vlib_rx)
{
	ethernet_header_t *eth;
	ip4_address_t *if_addr;
	ip_interface_address_t *ifa;

	if (arp->l2_type != ntohs(ETHERNET_ARP_HARDWARE_TYPE_ethernet) ||
	    arp->l3_type != ntohs(ETHERNET_TYPE_IP4))
		return;

	/* Check that IP address is local and matches incoming interface. */
	if_addr = ip4_interface_address_matching_destination(&ip4_main,
				&arp->ip4_over_ethernet[1].ip4,
				vlib_rx, &ifa);
	if (!if_addr)
		return;

	/* Source must also be local to subnet of matching interface address. */
	if (!ip4_destination_matches_interface(&ip4_main,
				&arp->ip4_over_ethernet[0].ip4, ifa))
		return;

	/* Reject replies with our local interface address. */
	if (if_addr->as_u32 == arp->ip4_over_ethernet[0].ip4.as_u32)
		return;

	if (if_addr->as_u32 != arp->ip4_over_ethernet[1].ip4.as_u32)
		return;

	eth = ethernet_buffer_get_header(b0);

	/* Trash ARP packets whose ARP-level source addresses do not
	 * match their L2-frame-level source addresses */
	if (memcmp(eth->src_address, arp->ip4_over_ethernet[0].ethernet,
	           sizeof(eth->src_address)))
		return;

	if (arp->ip4_over_ethernet[0].ip4.as_u32 == 0 ||
	    (arp->ip4_over_ethernet[0].ip4.as_u32 ==
	     arp->ip4_over_ethernet[1].ip4.as_u32))
		return;

	/* Learn or update sender's mapping only for requests or unicasts
	 * that don't match local interface address. */
	if (ethernet_address_cast(eth->dst_address) != ETHERNET_ADDRESS_UNICAST)
		return;

	vnet_arp_set_ip4_over_ethernet(rm.vnet_main, vlib_rx, ~0,
		                       &arp->ip4_over_ethernet[0], 0);
}

static uword
tap_inject_arp(vlib_main_t *m, vlib_node_runtime_t *node, vlib_frame_t *f)
{
	u32 n_left_from = f->n_vectors;
	u32 *from = vlib_frame_vector_args(f);
	u32 next_index = node->cached_next_index;
	u32 *to_next;
	u32 out = 0;

	while (n_left_from) {
		vlib_buffer_t *b0;
		u32 next0, bi0, n_left;
		u32 vlib_rx, vlib_tx;
		ethernet_arp_header_t *arp;

		vlib_get_next_frame(m, node, next_index, to_next, n_left);

		*(to_next++) = bi0 = *(from++);
		--n_left_from;
		--n_left;

		b0 = vlib_get_buffer(m, bi0);
		arp = vlib_buffer_get_current(b0);

		/* FIXME: What about VLAN? */
		b0->current_data -= sizeof(ethernet_header_t);
		b0->current_length += sizeof(ethernet_header_t);

		vlib_rx = vnet_buffer(b0)->sw_if_index[VLIB_RX];
		vlib_tx = rm.iface_to_tap[vlib_rx];
		vnet_buffer(b0)->sw_if_index[VLIB_TX] = vlib_tx;

		if (vlib_tx == 0 || vlib_tx == ~0)
			next0 = NEXT_ARP;
		else {
			next0 = NEXT_INJECT_ARP_ICMP;
			++out;
		}

		/* Add an ARP entry for injected replies. */
		if (next0 == NEXT_INJECT_ARP_ICMP &&
		    arp->opcode == ntohs(ETHERNET_ARP_OPCODE_reply))
			update_arp_entry(b0, arp, vlib_rx);

		vlib_validate_buffer_enqueue_x1(m, node, next_index, to_next,
		                                n_left, bi0, next0);
		vlib_put_next_frame(m, node, next_index, n_left);
	}

	vlib_node_increment_counter(m, tap_inject_arp_node.index,
	                            ERROR_INJECT_ARP, out);
	return f->n_vectors;
}

VLIB_REGISTER_NODE(tap_inject_arp_node) = {
	.function = tap_inject_arp,
	.name = "tap-inject-arp",
	.vector_size = sizeof(u32),
	.type = VLIB_NODE_TYPE_INTERNAL,
	.n_errors = ARRAY_LEN(error_strings),
	.error_strings = error_strings,
	.n_next_nodes = 2,
	.next_nodes = {
		[NEXT_ARP] = "arp-input",
		[NEXT_INJECT_ARP_ICMP] = "interface-output",
	},
};
