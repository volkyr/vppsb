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
#include <fcntl.h>              /* for open */
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>            /* for iovec */
#include <netinet/in.h>

#include <linux/socket.h>
#include <linux/if_arp.h>
#include <linux/if_tun.h>

#include <vlib/vlib.h>
#include <vnet/vnet.h>

#include <vlib/unix/unix.h>
#include <vnet/ethernet/ethernet.h>

#if DPDK == 1
#include <vnet/devices/dpdk/dpdk.h>
#endif

#include "turbotap.h"

static clib_error_t *
turbotap_delete_command_fn (vlib_main_t * vm,
                 unformat_input_t * input,
                 vlib_cli_command_t * cmd)
{
  turbotap_main_t * tr = &turbotap_main;
  u32 sw_if_index = ~0;

  if (tr->is_disabled)
    {
      return clib_error_return (0, "device disabled...");
    }

  if (unformat (input, "%U", unformat_vnet_sw_interface, tr->vnet_main,
                &sw_if_index))
      ;
  else
    return clib_error_return (0, "unknown input `%U'",
                              format_unformat_error, input);

  int rc = vnet_turbotap_delete (vm, sw_if_index);

  if (!rc) {
    vlib_cli_output (vm, "Deleted.");
  } else {
    vlib_cli_output (vm, "Error during deletion of tap interface. (rc: %d)", rc);
  }

  return 0;
}

VLIB_CLI_COMMAND (turbotap_delete_command, static) = {
  .path = "turbotap delete",
  .short_help = "turbotap delete <vpp-tap-intfc-name>",
  .function = turbotap_delete_command_fn,
};

static clib_error_t *
turbotap_connect_command_fn (vlib_main_t * vm,
                 unformat_input_t * input,
                 vlib_cli_command_t * cmd)
{
  u8 * intfc_name;
  turbotap_main_t * tr = &turbotap_main;
  u8 hwaddr[6];
  u8 *hwaddr_arg = 0;
  u32 sw_if_index;

  if (tr->is_disabled)
    {
      return clib_error_return (0, "device disabled...");
    }

  if (unformat (input, "%s", &intfc_name))
    ;
  else
    return clib_error_return (0, "unknown input `%U'",
                              format_unformat_error, input);

  if (unformat(input, "hwaddr %U", unformat_ethernet_address,
               &hwaddr))
    hwaddr_arg = hwaddr;

  int rv = vnet_turbotap_connect(vm, intfc_name, hwaddr_arg, &sw_if_index);
  if (rv) {
    switch (rv) {
    case VNET_API_ERROR_SYSCALL_ERROR_1:
      vlib_cli_output (vm, "Couldn't open /dev/net/turbotap");
      break;

    case VNET_API_ERROR_SYSCALL_ERROR_2:
      vlib_cli_output (vm, "Error setting flags on '%s'", intfc_name);
      break;

    case VNET_API_ERROR_SYSCALL_ERROR_3:
      vlib_cli_output (vm, "Couldn't open provisioning socket");
      break;

    case VNET_API_ERROR_SYSCALL_ERROR_4:
      vlib_cli_output (vm, "Couldn't get if_index");
      break;

    case VNET_API_ERROR_SYSCALL_ERROR_5:
      vlib_cli_output (vm, "Couldn't bind provisioning socket");
      break;

    case VNET_API_ERROR_SYSCALL_ERROR_6:
      vlib_cli_output (0, "Couldn't set device non-blocking flag");
      break;

    case VNET_API_ERROR_SYSCALL_ERROR_7:
      vlib_cli_output (0, "Couldn't set device MTU");
      break;

    case VNET_API_ERROR_SYSCALL_ERROR_8:
      vlib_cli_output (0, "Couldn't get interface flags");
      break;
    case VNET_API_ERROR_SYSCALL_ERROR_9:
      vlib_cli_output (0, "Couldn't set intfc admin state up");
      break;

    case VNET_API_ERROR_INVALID_REGISTRATION:
      vlib_cli_output (0, "Invalid registration");
      break;
    default:
      vlib_cli_output (0, "Unknown error: %d", rv);
      break;
    }
    return 0;
  }

  vlib_cli_output(vm, "%U\n", format_vnet_sw_if_index_name, vnet_get_main(), sw_if_index);
  return 0;
}

VLIB_CLI_COMMAND (turbotap_connect_command, static) = {
    .path = "turbotap connect",
    .short_help = "turbotap connect <intfc-name> [hwaddr <addr>]",
    .function = turbotap_connect_command_fn,
};

clib_error_t *
turbotap_cli_init (vlib_main_t * vm)
{
  return 0;
}

VLIB_INIT_FUNCTION (turbotap_cli_init);
