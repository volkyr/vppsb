/*
 * Copyright (c) 2015 Cisco and/or its affiliates.
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
 */

#define _GNU_SOURCE

#include <sys/socket.h>
#include <vlib/vlib.h>

#ifndef __TURBOTAP_H__
#define __TURBOTAP_H__

#define TUNGETSOCKFD _IOR('T', 224, int)
#define MAX_SEND 256
#define MAX_RECV 256

#define TAP_MTU_DEFAULT 1500
#define TAP_MTU_MAX 65535
#define TAP_MTU_MIN 68

#define foreach_turbotap_error                            \
  /* Must be first. */                                  \
 _(NONE, "no error")                                    \
 _(READ, "read error")                                  \
 _(BUFFER_ALLOCATION, "buffer allocation error")	\
 _(UNKNOWN, "unknown error")

typedef enum {
#define _(sym,str) TURBOTAP_ERROR_##sym,
  foreach_turbotap_error
#undef _
   TURBOTAP_N_ERROR,
 } turbotap_error_t;

typedef struct {
  u32 unix_fd;
  u32 unix_file_index;
  u32 provision_fd;
  u32 sw_if_index;              /* for counters */
  u32 hw_if_index;
  u32 is_promisc;
  struct ifreq ifr;
  u32 per_interface_next_index;
  u8 active;                    /* for delete */

  /* mtu count, mtu buffers */
  u32 mtu_bytes;
  u32 mtu_buffers;

  /* turbotap */
  int turbotap_fd;
  int sock_fd;
  int rx_ready;
  struct mmsghdr rx_msg[MAX_RECV];
  struct mmsghdr tx_msg[MAX_SEND];
} turbotap_interface_t;

typedef struct {
  /* Vector of VLIB rx buffers to use. */
  u32 * rx_buffers;

  /* record and put back unused rx buffers */
  u32 * unused_buffer_list;

  /*  Default MTU for newly created turbotap interface. */
  u32 mtu_bytes;

  /* Vector of tap interfaces */
  turbotap_interface_t * turbotap_interfaces;

  /* Vector of deleted tap interfaces */
  u32 * turbotap_inactive_interfaces;

  /* Bitmap of tap interfaces with pending reads */
  uword * pending_read_bitmap;

  /* Hash table to find turbotap interface given hw_if_index */
  uword * turbotap_interface_index_by_sw_if_index;

  /* Hash table to find turbotap interface given unix fd */
  uword * turbotap_interface_index_by_unix_fd;

  /* renumbering table */
  u32 * show_dev_instance_by_real_dev_instance;

  /* 1 => disable CLI */
  int is_disabled;

  /* convenience */
  vlib_main_t * vlib_main;
  vnet_main_t * vnet_main;
  unix_main_t * unix_main;
} turbotap_main_t;

extern vnet_device_class_t turbotap_dev_class;
extern vlib_node_registration_t turbotap_rx_node;
extern turbotap_main_t turbotap_main;

int vnet_turbotap_connect(vlib_main_t * vm, u8 * intfc_name, u8 *hwaddr_arg,
						u32 * sw_if_indexp);
int vnet_turbotap_delete(vlib_main_t *vm, u32 sw_if_index);

#endif
