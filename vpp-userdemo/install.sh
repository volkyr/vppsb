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

sysctl -w vm.nr_hugepages=1024
HUGEPAGES=`sysctl -n  vm.nr_hugepages`
if [ $HUGEPAGES != 1024 ]; then
    echo "ERROR: Unable to get 1024 hugepages, only got $HUGEPAGES.  Cannot finish."
    exit
fi

echo "deb https://nexus.fd.io/content/repositories/fd.io.stable.1701.ubuntu.trusty.main/ ./" | sudo tee -a /etc/apt/sources.list.d/99fd.io.list
apt-get -qq update
apt-get -qq install -y --force-yes vpp vpp-lib vpp-dpdk-dkms bridge-utils lxc
service vpp start

#Configure LXC network to create an inteface for Linux bridge and a unconsumed second inteface
echo -e "lxc.network.name=veth0\nlxc.network.type = veth\nlxc.network.name = veth_link1"  | sudo tee -a /etc/lxc/default.conf

lxc-checkconfig

containers=( cone ctwo )

for i in "${containers[@]}"
do
    sudo lxc-create -t download -n $i -- --dist ubuntu --release trusty --arch amd64
done

for i in "${containers[@]}"
do
    sudo lxc-start -n $i -d
done

function lxc_exec() {

    cntr="$1"
    rCMD="$2"

    CMD="sudo lxc-attach -n $cntr  -- $rCMD"

    echo "$CMD"
    eval "${CMD}"
}

for i in "${containers[@]}"
do
    lxc_exec $i "resolvconf -d eth0"
    lxc_exec $i "dhclient"

    lxc_exec $i "sh -c 'echo \"deb https://nexus.fd.io/content/repositories/fd.io.master.ubuntu.trusty.main/ ./\" >> /etc/apt/sources.list.d/99fd.io.list'"
    lxc_exec $i "apt-get -qq install -y wget"
    lxc_exec $i "apt-get -qq update"
    lxc_exec $i "apt-get -qq install -y --force-yes vpp"
    lxc_exec $i "sh -c 'echo  \"\\ndpdk {\\n   no-pci\\n}\" >> /etc/vpp/startup.conf'"
    lxc_exec $i "service vpp start"
done

for i in "${containers[@]}"
do
    sudo lxc-stop -n $i
done

#UI dependencies
curl -sL https://deb.nodesource.com/setup_4.x | sudo -E bash -
sudo apt-get install -y nodejs

#setup the npm proxy config correctly
if [[ -v http_proxy ]]; then
        sudo npm config set proxy $http_proxy
fi

if [[ -v https_proxy ]]; then
	npm_https_proxy=$(sed 's/https/http/' <<< $https_proxy)
        sudo npm config set https-proxy $npm_https_proxy
fi

#install nodeJS backend
sudo npm install /vagrant/ui/backend/
sudo mv node_modules/vppsb/node_modules/ /vagrant/ui/backend/
sudo npm install -g forever
sudo forever start /vagrant/ui/backend/server.js
