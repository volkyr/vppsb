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
#include <vlib/unix/unix.h>
#include <vnet/ethernet/ethernet.h>
#include <vnet/vnet.h>

#include <vnet/ip/ip.h>

#if DPDK == 1
#include <vnet/devices/dpdk/dpdk.h>
#endif

#include <turbotap/turbotap.h>

vlib_node_registration_t turbotap_rx_node;

enum {
  TURBOTAP_RX_NEXT_IP4_INPUT,
  TURBOTAP_RX_NEXT_IP6_INPUT,
  TURBOTAP_RX_NEXT_ETHERNET_INPUT,
  TURBOTAP_RX_NEXT_DROP,
  TURBOTAP_RX_N_NEXT,
};

typedef struct {
  u16 sw_if_index;
} turbotap_rx_trace_t;

u8 * format_turbotap_rx_trace (u8 * s, va_list * va)
{
  CLIB_UNUSED (vlib_main_t * vm) = va_arg (*va, vlib_main_t *);
  CLIB_UNUSED (vlib_node_t * node) = va_arg (*va, vlib_node_t *);
  vnet_main_t * vnm = vnet_get_main();
  turbotap_rx_trace_t * t = va_arg (*va, turbotap_rx_trace_t *);
  s = format (s, "%U", format_vnet_sw_if_index_name,
                vnm, t->sw_if_index);
  return s;
}

always_inline void
buffer_add_to_chain(vlib_main_t *vm, u32 bi, u32 first_bi, u32 prev_bi)
{
  vlib_buffer_t * b = vlib_get_buffer (vm, bi);
  vlib_buffer_t * first_b = vlib_get_buffer (vm, first_bi);
  vlib_buffer_t * prev_b = vlib_get_buffer (vm, prev_bi);

  /* update first buffer */
  first_b->total_length_not_including_first_buffer +=  b->current_length;

  /* update previous buffer */
  prev_b->next_buffer = bi;
  prev_b->flags |= VLIB_BUFFER_NEXT_PRESENT;

  /* update current buffer */
  b->next_buffer = 0;

#if DPDK > 0
  struct rte_mbuf * mbuf = rte_mbuf_from_vlib_buffer(b);
  struct rte_mbuf * first_mbuf = rte_mbuf_from_vlib_buffer(first_b);
  struct rte_mbuf * prev_mbuf = rte_mbuf_from_vlib_buffer(prev_b);
  first_mbuf->nb_segs++;
  prev_mbuf->next = mbuf;
  mbuf->data_len = b->current_length;
  mbuf->data_off = RTE_PKTMBUF_HEADROOM + b->current_data;
  mbuf->next = 0;
#endif
}

static uword
turbotap_rx_iface(vlib_main_t * vm,
           vlib_node_runtime_t * node,
           turbotap_interface_t * ti)
{
  turbotap_main_t * tr = &turbotap_main;
  const uword buffer_size = vlib_buffer_free_list_buffer_size ( vm,
                                  VLIB_BUFFER_DEFAULT_FREE_LIST_INDEX);
  u32 n_trace = vlib_get_trace_count (vm, node);
  u8 set_trace = 0;
  vnet_main_t *vnm;
  vnet_sw_interface_t * si;
  u8 admin_down;
  uword len = 0;
  u32 next_index =  TURBOTAP_RX_NEXT_ETHERNET_INPUT;
  u32 *to_next;

  vnm = vnet_get_main();
  si = vnet_get_sw_interface (vnm, ti->sw_if_index);
  admin_down = !(si->flags & VNET_SW_INTERFACE_FLAG_ADMIN_UP);

  if (ti->per_interface_next_index != ~0)
     next_index = ti->per_interface_next_index;

  /* Buffer Allocation */
  u32 desired_allocation = ti->rx_ready * ti->mtu_buffers + 32;
  if (PREDICT_TRUE(vec_len(tr->rx_buffers) < ti->rx_ready * ti->mtu_buffers))
    {
      len = vec_len(tr->rx_buffers);
      vec_validate(tr->rx_buffers, desired_allocation - 1);
      vec_validate(tr->unused_buffer_list, desired_allocation - 1);
      _vec_len(tr->unused_buffer_list) = 0;
      _vec_len(tr->rx_buffers) = len +
          vlib_buffer_alloc(vm, &tr->rx_buffers[len], desired_allocation - len);
      if (PREDICT_FALSE(vec_len(tr->rx_buffers) < ti->rx_ready * ti->mtu_buffers))
        {
          vlib_node_increment_counter(vm, turbotap_rx_node.index, TURBOTAP_ERROR_BUFFER_ALLOCATION, 1);
        }
    }

  /* Filling msgs */
  u32 i = 0;
  len = vec_len(tr->rx_buffers);
  while (i < ti->rx_ready && len > ti->mtu_buffers)
    {
      u32 j = 0;
      vec_validate(ti->rx_msg[i].msg_hdr.msg_iov, ti->mtu_buffers - 1);
      while (j < ti->mtu_buffers)
        {
          vlib_buffer_t *b = vlib_get_buffer(vm, tr->rx_buffers[len - 1]);
          ti->rx_msg[i].msg_hdr.msg_iov[j].iov_base = b->data;
          ti->rx_msg[i].msg_hdr.msg_iov[j].iov_len = buffer_size;
          len--;
          j++;
        }

      ti->rx_msg[i].msg_hdr.msg_iovlen = ti->mtu_buffers;
      ti->rx_msg[i].msg_hdr.msg_flags = MSG_DONTWAIT;
      ti->rx_msg[i].msg_hdr.msg_name = NULL;
      ti->rx_msg[i].msg_hdr.msg_namelen = 0;
      ti->rx_msg[i].msg_hdr.msg_control = NULL;
      ti->rx_msg[i].msg_hdr.msg_controllen = 0;
      ti->rx_msg[i].msg_len = 0;
      i++;
    }

  /*
   * Be careful here
   *
   * Experiments show that we need to set the time according
   * to the number of msgs receive from kernel even if the call
   * is NON-BLOCKING. If timeout is so small, then recvmmsg
   * call gets as many packets as it can in that time period.
   */
  struct timespec timeout = {.tv_sec = 0, .tv_nsec = 500000};
  int num_rx_msgs = recvmmsg(ti->sock_fd, ti->rx_msg, i, MSG_DONTWAIT, &timeout);
  if (num_rx_msgs <= 0) {
    if (errno != EAGAIN) {
      vlib_node_increment_counter(vm, turbotap_rx_node.index,
                                  TURBOTAP_ERROR_READ, 1);
    }
    return 0;
  }

  u32 next = next_index;
  u32 n_left_to_next = 0;

  i = 0;
  len = vec_len(tr->rx_buffers);  
  vlib_get_next_frame(vm, node, next_index, to_next, n_left_to_next);

  while (i != num_rx_msgs && n_left_to_next)
    {
      vlib_buffer_t *b0, *first_b0;
      u32 bi0 = 0, first_bi0 = 0, prev_bi0, j = 0;
      u32 bytes_to_put = 0, bytes_already_put = 0;
      u32 remain_len = ti->rx_msg[i].msg_len;

      while (remain_len && len)
        {        
          /* grab free buffer */
          prev_bi0 = bi0;
          bi0 = tr->rx_buffers[len - 1];
          b0 = vlib_get_buffer(vm, bi0);

	  bytes_to_put = remain_len > buffer_size ? buffer_size : remain_len;
          b0->current_length = bytes_to_put;

          if (bytes_already_put == 0)
            {
#if DPDK > 0
              struct rte_mbuf * mb = rte_mbuf_from_vlib_buffer(b0);
              rte_pktmbuf_data_len (mb) = b0->current_length;
              rte_pktmbuf_pkt_len (mb) = b0->current_length;
#endif
              b0->total_length_not_including_first_buffer = 0;
              b0->flags = VLIB_BUFFER_TOTAL_LENGTH_VALID;
              vnet_buffer(b0)->sw_if_index[VLIB_RX] = ti->sw_if_index;
              vnet_buffer(b0)->sw_if_index[VLIB_TX] = (u32)~0;
              first_bi0 = bi0;
              first_b0 = vlib_get_buffer(vm, first_bi0);
            }
          else
            buffer_add_to_chain(vm, bi0, first_bi0, prev_bi0);


          bytes_already_put += bytes_to_put;
          remain_len -= bytes_to_put;
          j++;
          len--;
        }

    /* record unused buffers */
    while (j < ti->mtu_buffers)
      {
        u32 vec_len_unused = vec_len(tr->unused_buffer_list);
        tr->unused_buffer_list[vec_len_unused] = tr->rx_buffers[len - 1];
        len--;
        j++;
        _vec_len(tr->unused_buffer_list) = vec_len_unused + 1;
      }

    /* trace */
    VLIB_BUFFER_TRACE_TRAJECTORY_INIT(first_b0);

    first_b0->error = node->errors[TURBOTAP_ERROR_NONE];

    /* Interface counters and tracing. */
    if (PREDICT_TRUE(!admin_down))
      {
        vlib_increment_combined_counter (
           vnet_main.interface_main.combined_sw_if_counters
           + VNET_INTERFACE_COUNTER_RX,
           os_get_cpu_number(), ti->sw_if_index,
           1, ti->rx_msg[i].msg_len);

        if (PREDICT_FALSE(n_trace > 0))
          {
            vlib_trace_buffer (vm, node, next_index,
                             first_b0, /* follow_chain */ 1);
            n_trace--;
            set_trace = 1;
            turbotap_rx_trace_t *t0 = vlib_add_trace (vm, node, first_b0, sizeof (*t0));
            t0->sw_if_index = si->sw_if_index;
          }
      } else {
        next = TURBOTAP_RX_NEXT_DROP;
      }

    /* next packet */
    to_next[0] = first_bi0;
    n_left_to_next -= 1;
    to_next +=1;

    /* enque and take next packet */
    vlib_validate_buffer_enqueue_x1(vm, node, next_index , to_next,
                            n_left_to_next, first_bi0, next);

    i++;
  }
    
  _vec_len(tr->rx_buffers) = len;
  vlib_put_next_frame(vm, node, next_index, n_left_to_next);

  /* put unused buffers back */
  while (vec_len(tr->unused_buffer_list) > 0)
    {
      u32 vec_len_unused = vec_len(tr->unused_buffer_list);
      u32 vec_len_rx = vec_len(tr->rx_buffers);
      tr->rx_buffers[vec_len_rx] = tr->unused_buffer_list[vec_len_unused - 1];
      _vec_len(tr->unused_buffer_list) -= 1;
      _vec_len(tr->rx_buffers) += 1;
    }

  if (ti->rx_ready - i > 0 )
    {
      ti->rx_ready -= i;
      if (ti->rx_ready < i)
	ti->rx_ready = i;
    }
  else if (ti->rx_ready + i > MAX_RECV)
        ti->rx_ready = MAX_RECV;
  else
        ti->rx_ready += i;

  if (set_trace)
    vlib_set_trace_count (vm, node, n_trace);
  return i;
}

static uword
turbotap_rx (vlib_main_t * vm,
           vlib_node_runtime_t * node,
           vlib_frame_t * frame)
{
  turbotap_main_t * tr = &turbotap_main;
  static u32 * ready_interface_indices;
  turbotap_interface_t * ti;
  int i;
  u32 total_count = 0;

  vec_reset_length (ready_interface_indices);
  clib_bitmap_foreach (i, tr->pending_read_bitmap,
  ({
    vec_add1 (ready_interface_indices, i);
  }));

  if (vec_len (ready_interface_indices) == 0)
    return 0;

  for (i = 0; i < vec_len(ready_interface_indices); i++)
    {
      tr->pending_read_bitmap =
        clib_bitmap_set (tr->pending_read_bitmap,
                         ready_interface_indices[i], 0);

      ti = vec_elt_at_index (tr->turbotap_interfaces, ready_interface_indices[i]);
      total_count += turbotap_rx_iface(vm, node, ti);
    }
  return total_count; //This might return more than 256.
}

static char * turbotap_rx_error_strings[] = {
#define _(sym,string) string,
  foreach_turbotap_error
#undef _
};

VLIB_REGISTER_NODE (turbotap_rx_node) = {
  .function = turbotap_rx,
  .name = "turbotap-rx",
  .type = VLIB_NODE_TYPE_INPUT,
  .state = VLIB_NODE_STATE_INTERRUPT,
  .vector_size = 4,
  .n_errors = TURBOTAP_N_ERROR,
  .error_strings = turbotap_rx_error_strings,
  .format_trace = format_turbotap_rx_trace,

  .n_next_nodes = TURBOTAP_RX_N_NEXT,
  .next_nodes = {
    [TURBOTAP_RX_NEXT_IP4_INPUT] = "ip4-input-no-checksum",
    [TURBOTAP_RX_NEXT_IP6_INPUT] = "ip6-input",
    [TURBOTAP_RX_NEXT_DROP] = "error-drop",
    [TURBOTAP_RX_NEXT_ETHERNET_INPUT] = "ethernet-input",
  },
};

