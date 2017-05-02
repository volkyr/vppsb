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

PACKAGE_REPO="https://nexus.fd.io/content/repositories/fd.io.stable.1704.ubuntu.xenial.main/"
HOME_DIR="/home/$USER"
RC_LOCAL="/etc/rc.local"
SSH_OPTIONS="-o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no"
APT_PROXY_CONF="/etc/apt/apt.conf.d/01proxy"
ENV_FILE="/etc/environment"
UNAMER=$(uname -r)

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

function add_to_rc_local()
{
    local str="$1"

    echo -e "$str" | sudo tee -a $RC_LOCAL
}

function sudo_exec() {

   CMD="$1"
   add_to_rc_local="${2:-0}"

   if [ "$add_to_rc_local" == "1" ]; then
       add_to_rc_local "$CMD"
   fi

   CMD="sudo $CMD"

   eval "${CMD}"
}

function lxc_exec() {

    cntr="$1"
    rCMD="$2"
    add_to_rc_local="${3:-0}"
 
    CMD="lxc-attach -n $cntr  -- $rCMD"

    echo "$CMD"
    sudo_exec "$CMD" $add_to_rc_local
}

function get_field() {
    file="$1"
    field="$2"

    value=$(grep $field $file | awk -F : '{print $2}' | sed -e 's/^[ ]*//' | sed -e 's/kernver/"$UNAMER"/')
    echo $value
}

echo "deb $PACKAGE_REPO ./" | sudo tee -a /etc/apt/sources.list.d/99fd.io.list
sudo apt-get -qq update
sudo apt-get -qq install -y --force-yes linux-image-extra-$(uname -r) lxc bridge-utils tmux
sudo apt-get -qq install -y --force-yes vpp vpp vpp-dpdk-dkms vpp-plugins

#Disable DPDK to make memory requirements more modest
sudo sed -i_dpdk '47,52d' /etc/vpp/startup.conf
echo -e "plugins {\n\tplugin dpdk_plugin.so { disable }\n}" | sudo tee -a /etc/vpp/startup.conf

#Fix VPP on the host to use 32 hugepages
echo -e "heapsize 64M" | sudo tee -a /etc/vpp/startup.conf
sudo sed -i 's/vm.nr_hugepages=1024/vm.nr_hugepages=32/' /etc/sysctl.d/80-vpp.conf
sudo sed -i 's/kernel.shmmax=2147483648/kernel.shmmax=67018864/' /etc/sysctl.d/80-vpp.conf

#Provision containers with two network connections, second connection is unconnected
echo -e "lxc.network.name=veth0" | sudo tee -a /etc/lxc/default.conf
echo -e "lxc.network.type = veth" | sudo tee -a /etc/lxc/default.conf
echo -e "lxc.network.hwaddr = 00:17:3e:xx:xx:xx\n" | sudo tee -a /etc/lxc/default.conf
echo -e "lxc.network.name=veth_link1" | sudo tee -a /etc/lxc/default.conf

sudo lxc-checkconfig

# update rc.local to be interpreted with bash
sudo sed -i '1 s/^.*$/#!\/bin\/bash/g' $RC_LOCAL
# remove the exit 0 from rc.local.
sudo sed -i 's/exit 0//' $RC_LOCAL

# add rename_veth_interface to /etc/rc.local
read -r -d '' TMP_RVI <<'EOF'
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
EOF
add_to_rc_local "$TMP_RVI"

# For the moment just cross connect the host, will more clever later.
read -r -d '' TMP_CCI <<'EOF'
function cross_connect_interfaces() {

   sudo vppctl create host-interface name veth-cone
   sudo vppctl create host-interface name veth-ctwo
   sudo vppctl set int l2 xconnect host-veth-cone host-veth-ctwo
   sudo vppctl set int l2 xconnect host-veth-ctwo host-veth-cone
   sudo vppctl set int state host-veth-cone up
   sudo vppctl set int state host-veth-ctwo up
}
EOF
add_to_rc_local "$TMP_CCI"

ssh-keygen -t rsa -b 1024 -N "" -f ~/.ssh/id_rsa
openssh_pubkey=`cat ~/.ssh/id_rsa.pub`

#Ensure that virtual bridge comes up after boot
add_to_rc_local "#autostart vpp on the host"
sudo_exec "service vpp start" 1

for f in $(ls /vagrant/containers/*.cntr)
do
    i=$(basename $f | sed s/.cntr//)
    dist=$(get_field $f DIST)
    ver=$(get_field $f VER)
    packages=$(get_field $f PACKAGES)
    pip=$(get_field $f PIP)
    provision_file="/vagrant/containers/"$i".provision.sh"
 
    sudo lxc-create -t download -n $i -- --dist $dist --release $ver --arch amd64

    #autostart container after a reboot (standard lxc way doesn't work).
    add_to_rc_local "#autostart container $i"

    sudo_exec "lxc-start -n $i -d" 1
	
    lxc_exec $i "resolvconf -d veth0"

    #dhcp after boot
    lxc_exec $i "dhclient veth0" 1

    #insert delay to allow completion before starting ssh service
    add_to_rc_local "sleep 1"

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

    lxc_exec $i "service ssh restart" 1

    ip_address=$(sudo lxc-ls -f | grep $i | awk '{print $5}')
    echo $ip_address $i | sudo tee -a /etc/hosts

    if [ -s $APT_PROXY_CONF ]
    then
        scp $SSH_OPTIONS $APT_PROXY_CONF root@$i:$APT_PROXY_CONF
    fi

    if [ -s $ENV_FILE ]
    then
        scp $SSH_OPTIONS $ENV_FILE root@$i:$ENV_FILE
    fi

    #rename the backend interface to something sensible
    rename_veth_interface $i "veth-$i"
    add_to_rc_local "rename_veth_interface $i 'veth-$i'"

    if [ -s $provision_file ]
    then
       tmpname=$(mktemp)".sh"
       scp $SSH_OPTIONS $provision_file $USER@$i:$tmpname
       ssh $SSH_OPTIONS $USER@$i "sh -c $tmpname"
    fi

    #install any pip packages
    if [ ! -z "$pip" ]
    then
       ssh -t $SSH_OPTIONS $USER@$i "sudo -E pip install $pip"
    fi

done

#cross connect the containers
add_to_rc_local "sleep 1"
add_to_rc_local "cross_connect_interfaces"

add_to_rc_local "exit 0"

#setting password to username
echo "$USER:$USER" | sudo chpasswd

echo -e "List of containers deployed in the dev environment:" | sudo tee -a /etc/motd
for f in $(ls /vagrant/containers/*.cntr)
do
     i=$(basename $f | sed s/.cntr//)
     desc=$(get_field $f DESC)
     echo -e $i":\t"$desc | sudo tee -a /etc/motd
done

echo "To access the environment, type 'vagrant ssh'"
