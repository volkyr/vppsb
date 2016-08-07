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

if [ $USER != "root" ] ; then
    #echo "Restarting script with sudo..."
    sudo $0 ${*}
    exit
fi

C1_IP=$1
C1_GW=$2
C2_IP=$3
C2_GW=$4

# Restart VPP so there is no config
service vpp restart

# delete previous incarnations if they exist
ip link del dev veth_link1 &>/dev/null
ip link del dev veth_link2 &>/dev/null
ip netns del c1 &>/dev/null
ip netns del c2 &>/dev/null

#create namespaces
ip netns add c1 &>/dev/null
ip netns add c2 &>/dev/null

# create and configure 1st veth pair
ip link add name veth_link1 type veth peer name link1
ip link set dev link1 up
ip link set dev veth_link1 up netns c1

ip netns exec c1 \
     bash -c "
    ip link set dev lo up
    ip addr add ${C1_IP} dev veth_link1
    ip route add default via ${C1_GW} dev veth_link1
"

# create and configure 2nd veth pair
ip link add name veth_link2 type veth peer name link2
ip link set dev link2 up
ip link set dev veth_link2 up netns c2

ip netns exec c2 \
     bash -c "
    ip link set dev lo up
    ip addr add ${C2_IP} dev veth_link2
    ip route add default via ${C2_GW} dev veth_link2
"
