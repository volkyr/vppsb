/*
 *------------------------------------------------------------------
 * Copyright (c) 2016 Cisco and/or its affiliates.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *------------------------------------------------------------------
 */

#define _GNU_SOURCE

#include <stdint.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>            /* for iovec */
#include <netinet/in.h>

#include <vlib/vlib.h>
#include <vnet/ip/ip.h>
#include <vlib/unix/unix.h>
#include <vnet/ethernet/ethernet.h>

#if DPDK == 1
#include <vnet/devices/dpdk/dpdk.h>
#endif

#include "turbotap.h"

vnet_device_class_t turbotap_dev_class;

static u8 * format_turbotap_interface_name (u8 * s, va_list * args)
{ 
  u32 i = va_arg (*args, u32);
  u32 show_dev_instance = ~0;
  turbotap_main_t * tr = &turbotap_main;
  
  if (i < vec_len (tr->show_dev_instance_by_real_dev_instance))
    show_dev_instance = tr->show_dev_instance_by_real_dev_instance[i];
  
  if (show_dev_instance != ~0)
    i = show_dev_instance;
  
  s = format (s, "turbotap-%d", i);
  return s;
}

static void turbotap_set_interface_next_node (vnet_main_t *vnm,
                                            u32 hw_if_index,
                                            u32 node_index)
{
  turbotap_main_t *tr = &turbotap_main;
  turbotap_interface_t *ti;
  vnet_hw_interface_t *hw = vnet_get_hw_interface (vnm, hw_if_index);

  ti = vec_elt_at_index (tr->turbotap_interfaces, hw->dev_instance);

  /* Shut off redirection */
  if (node_index == ~0)
    {
      ti->per_interface_next_index = node_index;
      return;
    }

  ti->per_interface_next_index =
    vlib_node_add_next (tr->vlib_main, turbotap_rx_node.index, node_index);
}

static_always_inline uword
turbotap_tx_iface(vlib_main_t * vm,
                vlib_node_runtime_t * node,
                vlib_frame_t * frame,
                turbotap_interface_t * ti)
{
  u32 * buffers = vlib_frame_args (frame);
  uword n_packets = frame->n_vectors;
  vlib_buffer_t * b;
  int i = 0;

  vnet_sw_interface_t *si = vnet_get_sw_interface (vnet_get_main(), ti->sw_if_index);
  if (PREDICT_FALSE(!(si->flags & VNET_SW_INTERFACE_FLAG_ADMIN_UP))) {
    //Drop if interface is down
    vlib_buffer_free(vm, vlib_frame_vector_args(frame), frame->n_vectors);
    return 0;
  }

  u32 n_tx = (n_packets > MAX_SEND)?MAX_SEND:n_packets;
  u32 total_bytes = 0;
  for (i = 0; i < n_tx; i++) {
    struct iovec * iov;
    b = vlib_get_buffer(vm, buffers[i]);

    if (ti->tx_msg[i].msg_hdr.msg_iov)
      _vec_len(ti->tx_msg[i].msg_hdr.msg_iov) = 0; //Reset vector

    /* VLIB buffer chain -> Unix iovec(s). */
    vec_add2 (ti->tx_msg[i].msg_hdr.msg_iov, iov, 1);
    iov->iov_base = b->data + b->current_data;
    iov->iov_len = b->current_length;
    ti->tx_msg[i].msg_len = b->current_length;
    if (PREDICT_FALSE (b->flags & VLIB_BUFFER_NEXT_PRESENT)) {
      do {
        b = vlib_get_buffer (vm, b->next_buffer);
        vec_add2 (ti->tx_msg[i].msg_hdr.msg_iov, iov, 1);
        iov->iov_base = b->data + b->current_data;
        iov->iov_len = b->current_length;
        ti->tx_msg[i].msg_len += b->current_length;
      } while (b->flags & VLIB_BUFFER_NEXT_PRESENT);
    }

    ti->tx_msg[i].msg_hdr.msg_name = NULL;
    ti->tx_msg[i].msg_hdr.msg_namelen = 0;
    ti->tx_msg[i].msg_hdr.msg_iovlen = _vec_len(ti->tx_msg[i].msg_hdr.msg_iov);
    ti->tx_msg[i].msg_hdr.msg_control = NULL;
    ti->tx_msg[i].msg_hdr.msg_controllen = 0;
    ti->tx_msg[i].msg_hdr.msg_flags = MSG_DONTWAIT;
    total_bytes += ti->tx_msg[i].msg_len;
  }

  if (n_tx) {
    int tx;
    if ((tx = sendmmsg(ti->sock_fd, ti->tx_msg, n_tx, MSG_DONTWAIT)) < 1) {
      vlib_increment_simple_counter
      (vnet_main.interface_main.sw_if_counters
       + VNET_INTERFACE_COUNTER_TX_ERROR, os_get_cpu_number(),
       ti->sw_if_index, n_tx);
    } else {
      vlib_increment_combined_counter(
          vnet_main.interface_main.combined_sw_if_counters
          + VNET_INTERFACE_COUNTER_TX,
          os_get_cpu_number(), ti->sw_if_index,
          tx, total_bytes);
    }
  }

  vlib_buffer_free(vm, vlib_frame_vector_args(frame), frame->n_vectors);
  return n_packets;
}

/*
 * turbotap_tx
 * Output node, writes the buffers comprising the incoming frame 
 * to the tun/tap device, aka hands them to the Linux kernel stack.
 * 
 */
static uword
turbotap_tx (vlib_main_t * vm,
           vlib_node_runtime_t * node,
           vlib_frame_t * frame)
{
  u32 * buffers = vlib_frame_args (frame);
  turbotap_main_t * tr = &turbotap_main;
  turbotap_interface_t * ti;

  if (!frame->n_vectors)
    return 0;

  vlib_buffer_t *b = vlib_get_buffer(vm, buffers[0]);
  u32 tx_sw_if_index = vnet_buffer(b)->sw_if_index[VLIB_TX];
  if (tx_sw_if_index == (u32)~0)
    tx_sw_if_index = vnet_buffer(b)->sw_if_index[VLIB_RX];

  ASSERT(tx_sw_if_index != (u32)~0);

  /* Use the sup intfc to finesse vlan subifs */
  vnet_hw_interface_t *hw = vnet_get_sup_hw_interface (tr->vnet_main, tx_sw_if_index);
  tx_sw_if_index = hw->sw_if_index;

  uword * p = hash_get (tr->turbotap_interface_index_by_sw_if_index,
                        tx_sw_if_index);
  if (p == 0) {
    clib_warning ("sw_if_index %d unknown", tx_sw_if_index);
    return 0;
  } else {
    ti = vec_elt_at_index (tr->turbotap_interfaces, p[0]);
  }

  return turbotap_tx_iface(vm, node, frame, ti);
}

VLIB_REGISTER_NODE (turbotap_tx_node,static) = {
  .function = turbotap_tx,
  .name = "turbotap-tx",
  .type = VLIB_NODE_TYPE_INTERNAL,
  .vector_size = 4,
};

/* 
 * Mainly exists to set link_state == admin_state
 * otherwise, e.g. ip6 neighbor discovery breaks
 */
static clib_error_t *
turbotap_interface_admin_up_down (vnet_main_t * vnm, u32 hw_if_index, u32 flags)
{
  uword is_admin_up = (flags & VNET_SW_INTERFACE_FLAG_ADMIN_UP) != 0;
  u32 hw_flags;
  u32 speed_duplex = VNET_HW_INTERFACE_FLAG_FULL_DUPLEX
    | VNET_HW_INTERFACE_FLAG_SPEED_40G;

  if (is_admin_up)
    hw_flags = VNET_HW_INTERFACE_FLAG_LINK_UP | speed_duplex;
  else
    hw_flags = speed_duplex;

  vnet_hw_interface_set_flags (vnm, hw_if_index, hw_flags);
  return 0;
}

VNET_DEVICE_CLASS (turbotap_dev_class) = {
  .name = "turbotap",
  .tx_function = turbotap_tx,
  .format_device_name = format_turbotap_interface_name,
  .rx_redirect_to_node = turbotap_set_interface_next_node,
  .admin_up_down_function = turbotap_interface_admin_up_down,
  .no_flatten_output_chains = 1,
};

