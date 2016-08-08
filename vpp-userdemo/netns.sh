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

cone_IP=$1
cone_GW=$2
ctwo_IP=$3
ctwo_GW=$4

containers=( cone ctwo )
idx=1

# Restart VPP so there is no config
service vpp restart

for cntr in "${containers[@]}"
do
    #stop the container
    echo "stopping container $cntr"
    sudo lxc-stop -n $cntr
done

# LXC gives backend interfaces horrible names, give them a better name.
function rename_veth_interface() {

    local cntr="$1"
    local nifname="$2"

    ifr_index=`sudo lxc-attach -n $cntr -- ip -o link | tail -n 1 | awk -F : '{print $1}'`
    ifr_index=$((ifr_index+1))

    for dir in /sys/class/net/*/
    do
        ifindex=`cat $dir/ifindex`
        if [ $ifindex == $ifr_index ]
            then ifname=`basename $dir`
        fi
    done

    sudo ip link set $ifname down
    sudo ip link set $ifname name $nifname
    sudo ip link set $nifname up
}

for cntr in "${containers[@]}"
do
    cip=$cntr\_IP
    cgw=$cntr\_GW

    #start the container
    sudo lxc-start -n $cntr -d

    #give the backend of our veth a sensible name.
    rename_veth_interface $cntr "link"$idx

    #start vpp in the container.
    #sudo lxc-attach -n $cntr service vpp start

    #setup the inteface in the container 
    sudo lxc-attach -n $cntr -- \
        bash -c "
        ip link set dev lo up
        ip addr add ${!cip} dev veth_link1
        ip link set dev veth_link1 up
        ip route add default via ${!cgw} dev veth_link1"

    idx=$((idx+1))
done
