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

#include "tap_inject.h"

#include <librtnl/netns.h>
#include <vlibmemory/api.h>
#include <vnet/ethernet/arp_packet.h>
#include <vnet/ip/ip6_neighbor.h>

#include <vnet/ip/ip.h>
#include <vnet/ip/lookup.h>
#ifdef ip6_add_del_route_next_hop
#define FIB_VERSION 1
#else
#include <vnet/fib/fib.h>
#define FIB_VERSION 2
#endif

static void
add_del_addr (ns_addr_t * a, int is_del)
{
  vlib_main_t * vm = vlib_get_main ();
  u32 sw_if_index;

  sw_if_index = tap_inject_lookup_sw_if_index_from_tap_if_index (
                        a->ifaddr.ifa_index);

  if (sw_if_index == ~0)
    return;

  if (a->ifaddr.ifa_family == AF_INET)
    {
      ip4_add_del_interface_address (vm, sw_if_index,
          (ip4_address_t *) a->local, a->ifaddr.ifa_prefixlen, is_del);
    }
  else if (a->ifaddr.ifa_family == AF_INET6)
    {
      ip6_add_del_interface_address (vm, sw_if_index,
          (ip6_address_t *) a->addr, a->ifaddr.ifa_prefixlen, is_del);
    }
}


struct set_flags_args {
  u32 index;
  u8 flags;
};

static void
set_flags_cb (struct set_flags_args * a)
{
  vnet_sw_interface_set_flags (vnet_get_main (), a->index, a->flags);
}

static void
add_del_link (ns_link_t * l, int is_del)
{
  struct set_flags_args args = { ~0, 0 };
  vnet_sw_interface_t * sw;
  u8 flags = 0;
  u32 sw_if_index;

  sw_if_index = tap_inject_lookup_sw_if_index_from_tap_if_index (
                        l->ifi.ifi_index);

  if (sw_if_index == ~0)
    return;

  sw = vnet_get_sw_interface (vnet_get_main (), sw_if_index);

  flags = sw->flags;

  if (l->ifi.ifi_flags & IFF_UP)
    flags |= VNET_SW_INTERFACE_FLAG_ADMIN_UP;
  else
    flags &= ~VNET_SW_INTERFACE_FLAG_ADMIN_UP;

  args.index = sw_if_index;
  args.flags = flags;

  vl_api_rpc_call_main_thread (set_flags_cb, (u8 *)&args, sizeof (args));
}


static void
add_del_neigh (ns_neigh_t * n, int is_del)
{
  vnet_main_t * vnet_main = vnet_get_main ();
  vlib_main_t * vm = vlib_get_main ();
  u32 sw_if_index;

  sw_if_index = tap_inject_lookup_sw_if_index_from_tap_if_index (
                        n->nd.ndm_ifindex);

  if (sw_if_index == ~0)
    return;

  if (n->nd.ndm_family == AF_INET)
    {
      ethernet_arp_ip4_over_ethernet_address_t a;

      memset (&a, 0, sizeof (a));

      clib_memcpy (&a.ethernet, n->lladdr, ETHER_ADDR_LEN);
      clib_memcpy (&a.ip4, n->dst, sizeof (a.ip4));


      if (n->nd.ndm_state & NUD_REACHABLE)
        {
#if FIB_VERSION == 1
        vnet_arp_set_ip4_over_ethernet (vnet_main, sw_if_index, ~0, &a, 0);
#else
        vnet_arp_set_ip4_over_ethernet (vnet_main, sw_if_index,
                                        &a, 0 /* static */ ,
                                        0 /* no fib entry */);

#endif /* FIB_VERSION == 1 */
        }
      else if (n->nd.ndm_state & NUD_FAILED)
        {
#if FIB_VERSION == 1
        vnet_arp_unset_ip4_over_ethernet (vnet_main, sw_if_index, ~0, &a);
#else
        vnet_arp_unset_ip4_over_ethernet (vnet_main, sw_if_index, &a);
#endif /* FIB_VERSION == 1 */
        }
    }
  else if (n->nd.ndm_family == AF_INET6)
    {
      if (n->nd.ndm_state & NUD_REACHABLE)
	{
#if FIB_VERSION == 1
        vnet_set_ip6_ethernet_neighbor (vm, sw_if_index,
            (ip6_address_t *) n->dst, n->lladdr, ETHER_ADDR_LEN, 0);
#else
        vnet_set_ip6_ethernet_neighbor (vm, sw_if_index,
            (ip6_address_t *) n->dst, n->lladdr, ETHER_ADDR_LEN,
            0 /* static */,
            0 /* no fib entry */);
#endif /* FIB_VERSION == 1 */
	}
      else
        vnet_unset_ip6_ethernet_neighbor (vm, sw_if_index,
            (ip6_address_t *) n->dst, n->lladdr, ETHER_ADDR_LEN);
    }
}


#define TAP_INJECT_HOST_ROUTE_TABLE_MAIN 254

static void
add_del_route (ns_route_t * r, int is_del)
{
  u32 sw_if_index;

  sw_if_index = tap_inject_lookup_sw_if_index_from_tap_if_index (r->oif);

  if (sw_if_index == ~0 || r->table != TAP_INJECT_HOST_ROUTE_TABLE_MAIN)
    return;

  if (r->rtm.rtm_family == AF_INET)
    {
#if FIB_VERSION == 1
      ip4_add_del_route_next_hop (&ip4_main,
          is_del ? IP4_ROUTE_FLAG_DEL : IP4_ROUTE_FLAG_ADD,
          (ip4_address_t *) r->dst, r->rtm.rtm_dst_len,
          (ip4_address_t *) r->gateway, sw_if_index, 0, ~0, 0);
#else
	    fib_prefix_t prefix;
	    ip46_address_t nh;

	    memset (&prefix, 0, sizeof (prefix));
	    prefix.fp_len = r->rtm.rtm_dst_len;
	    prefix.fp_proto = FIB_PROTOCOL_IP4;
	    clib_memcpy (&prefix.fp_addr.ip4, r->dst, sizeof (prefix.fp_addr.ip4));

	    memset (&nh, 0, sizeof (nh));
	    clib_memcpy (&nh.ip4, r->gateway, sizeof (nh.ip4));

	    fib_table_entry_path_add (0, &prefix, FIB_SOURCE_API,
	                              FIB_ENTRY_FLAG_NONE, prefix.fp_proto,
	                              &nh, sw_if_index, 0,
	                              0 /* weight */, NULL,
	                              FIB_ROUTE_PATH_FLAG_NONE);
#endif /* FIB_VERSION == 1 */
    }
  else if (r->rtm.rtm_family == AF_INET6)
    {
#if FIB_VERSION == 1
      ip6_add_del_route_next_hop (&ip6_main,
          is_del ? IP6_ROUTE_FLAG_DEL : IP6_ROUTE_FLAG_ADD,
          (ip6_address_t *) r->dst, r->rtm.rtm_dst_len,
          (ip6_address_t *) r->gateway, sw_if_index, 0, ~0, 0);
#else
	    fib_prefix_t prefix;
	    ip46_address_t nh;

	    memset (&prefix, 0, sizeof (prefix));
	    prefix.fp_len = r->rtm.rtm_dst_len;
	    prefix.fp_proto = FIB_PROTOCOL_IP6;
	    clib_memcpy (&prefix.fp_addr.ip6, r->dst, sizeof (prefix.fp_addr.ip6));

	    memset (&nh, 0, sizeof (nh));
	    clib_memcpy (&nh.ip6, r->gateway, sizeof (nh.ip6));

	    fib_table_entry_path_add (0, &prefix, FIB_SOURCE_API,
	                              FIB_ENTRY_FLAG_NONE, prefix.fp_proto,
	                              &nh, sw_if_index, 0,
	                              0 /* weight */, NULL,
	                              FIB_ROUTE_PATH_FLAG_NONE);
#endif /* FIB_VERSION == 1 */
    }
}


static void
netns_notify_cb (void * obj, netns_type_t type, u32 flags, uword opaque)
{
  if (type == NETNS_TYPE_ADDR)
    add_del_addr ((ns_addr_t *)obj, flags & NETNS_F_DEL);

  else if (type == NETNS_TYPE_LINK)
    add_del_link ((ns_link_t *)obj, flags & NETNS_F_DEL);

  else if (type == NETNS_TYPE_NEIGH)
    add_del_neigh ((ns_neigh_t *)obj, flags & NETNS_F_DEL);

  else if (type == NETNS_TYPE_ROUTE)
    add_del_route ((ns_route_t *)obj, flags & NETNS_F_DEL);
}

void
tap_inject_enable_netlink (void)
{
  char nsname = 0;
  netns_sub_t sub = {
    .notify = netns_notify_cb,
    .opaque = 0,
  };

  netns_open (&nsname, &sub);
}
