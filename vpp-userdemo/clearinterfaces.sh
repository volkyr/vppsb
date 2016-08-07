#!/usr/bin/env bash

# Copyright (c) 2016 Cisco and/or its affiliates.
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at:
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


# Capture all the interface IPs, in case we need them later
ip -o addr show > ~vagrant/ifconfiga
chown vagrant:vagrant ~vagrant/ifconfiga

# Disable all ethernet interfaces other than the default route
# interface so VPP will use those interfaces.  The VPP auto-blacklist
# algorithm prevents the use of any physical interface contained in the
# routing table (i.e. "route --inet --inet6") preventing the theft of
# the management ethernet interface by VPP from the kernel.
for intf in $(ls /sys/class/net) ; do
    if [ -d /sys/class/net/$intf/device ] &&
        [ "$(route --inet --inet6 | grep default | grep $intf)" == "" ] ; then
        ifconfig $intf down
    fi
done
