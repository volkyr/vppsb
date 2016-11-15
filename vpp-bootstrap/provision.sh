#!/usr/bin/env bash
#
# Copyright (c) 2016 Intel Corporation
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

HOME_DIR="/home/$USER"
SSH_OPTIONS="-o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no"
APT_PROXY_CONF="/etc/apt/apt.conf.d/01proxy"
ENV_FILE="/etc/environment"
UNAMER=$(uname -r)
DEV_BRIDGE="lxcbr1"

sudo sysctl -w vm.nr_hugepages=1024
HUGEPAGES=`sudo sysctl -n  vm.nr_hugepages`
if [ $HUGEPAGES != 1024 ]; then
    echo "ERROR: Unable to get 1024 hugepages, only got $HUGEPAGES.  Cannot finish."
    exit
fi

sudo apt-get -qq update
sudo apt-get -qq install -y --force-yes lxc bridge-utils

echo -e "lxc.network.name=veth0" | sudo tee -a /etc/lxc/default.conf
echo -e "lxc.network.type = veth" | sudo tee -a /etc/lxc/default.conf
echo -e "lxc.network.link = lxcbr1" | sudo tee -a /etc/lxc/default.conf
echo -e "lxc.network.flags = up" | sudo tee -a /etc/lxc/default.conf
echo -e "lxc.network.hwaddr = 00:17:3e:xx:xx:xx\n" | sudo tee -a /etc/lxc/default.conf
echo -e "lxc.network.name=veth_link1" | sudo tee -a /etc/lxc/default.conf

sudo lxc-checkconfig

sudo brctl addbr $DEV_BRIDGE
sudo ip link set $DEV_BRIDGE up

ssh-keygen -t rsa -b 1024 -N "" -f ~/.ssh/id_rsa
openssh_pubkey=`cat ~/.ssh/id_rsa.pub`

function lxc_exec() {

    cntr="$1"
    rCMD="$2"

    CMD="sudo lxc-attach -n $cntr  -- $rCMD"

    echo "$CMD"
    eval "${CMD}"
}

function get_field() {
    file="$1"
    field="$2"
    
    value=$(grep $field $file | awk -F : '{print $2}' | sed -e 's/^[ ]*//' | sed -e 's/kernver/"$UNAMER"/')
    echo $value
}

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

for f in $(ls /vagrant/containers/*.cntr)
do
    i=$(basename $f | sed s/.cntr//)
    dist=$(get_field $f DIST)
    ver=$(get_field $f VER)
    packages=$(get_field $f PACKAGES)
    provision_file="/vagrant/containers/"$i".provision.sh"
 
    sudo lxc-create -t download -n $i -- --dist $dist --release $ver --arch amd64
    sudo lxc-start -n $i -d
	
    lxc_exec $i "resolvconf -d veth0"
    lxc_exec $i "dhclient veth0"

    lxc_exec $i "apt-get -qq install -y git openssh-server"
    lxc_exec $i "apt-get -qq update"

    lxc_exec $i "adduser --disabled-password --gecos \"\" $USER"

    lxc_exec $i "mkdir -p /root/.ssh/"
    lxc_exec $i "mkdir -p $HOME_DIR/.ssh/"

    lxc_exec $i "sh -c 'echo $openssh_pubkey >> /root/.ssh/authorized_keys'"
    lxc_exec $i "sh -c 'echo $openssh_pubkey >> $HOME_DIR/.ssh/authorized_keys'"

    lxc_exec $i "chmod 0600 /root/.ssh/authorized_keys"
    lxc_exec $i "chmod 0600 $HOME_DIR/.ssh/authorized_keys"

    lxc_exec $i "chown -R $USER.$USER $HOME_DIR/.ssh/"

    lxc_exec $i "sh -c 'echo \"%$USER ALL=(ALL) NOPASSWD: ALL\" > /etc/sudoers.d/10_$USER'"

    lxc_exec $i "update-alternatives --install /bin/sh sh /bin/bash 100"

    lxc_exec $i "apt-get -qq install $packages"

    ip_address=$(sudo lxc-ls -f | grep $i | awk '{print $3}')
    echo $ip_address $i | sudo tee -a /etc/hosts

    if [ -s $APT_PROXY_CONF ]
    then
        scp $SSH_OPTIONS $APT_PROXY_CONF root@$i:$APT_PROXY_CONF
    fi

    if [ -s $ENV_FILE ]
    then
        scp $SSH_OPTIONS $ENV_FILE root@$i:$ENV_FILE
    fi

#    backend_intr="link_"$cntr
#    rename_veth_interface $i $backend_intr
#    sudo brctl addif $DEV_BRIDGE $backend_intr

    if [ -s $provision_file ]
    then
       tmpname=$(mktemp)".sh"
       scp $SSH_OPTIONS $provision_file $USER@$i:$tmpname
       ssh $SSH_OPTIONS $USER@$i "sh -c $tmpname"
    fi	
done

echo "List of containers deployed in the dev environment:"
for f in $(ls /vagrant/containers/*.cntr)
do
     i=$(basename $f | sed s/.cntr//)
     desc=$(get_field $f DESC)
     echo $i":\t"$desc
done

echo "To get access the dev environment, type 'vagrant ssh'"
