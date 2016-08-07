#!/bin/bash
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

# Make sure that we get the hugepages we need on provision boot
# Note: The package install should take care of this at the end
#       But sometimes after all the work of provisioning, we can't
#       get the requested number of hugepages without rebooting.
#       So do it here just in case
sysctl -w vm.nr_hugepages=1024
HUGEPAGES=`sysctl -n  vm.nr_hugepages`
if [ $HUGEPAGES != 1024 ]; then
    echo "ERROR: Unable to get 1024 hugepages, only got $HUGEPAGES.  Cannot finish."
    exit
fi

exit 0

# Figure out what system we are running on
if [ -f /etc/lsb-release ];then
    . /etc/lsb-release
elif [ -f /etc/redhat-release ];then
    yum install -y redhat-lsb
    DISTRIB_ID=`lsb_release -si`
    DISTRIB_RELEASE=`lsb_release -sr`
    DISTRIB_CODENAME=`lsb_release -sc`
    DISTRIB_DESCRIPTION=`lsb_release -sd`
fi

# Do initial setup for the system
if [ $DISTRIB_ID == "Ubuntu" ]; then
    # Fix grub-pc on Virtualbox with Ubuntu
    export DEBIAN_FRONTEND=noninteractive

    # Standard update + upgrade dance
    apt-get update
    apt-get upgrade -y

    # Fix the silly notion that /bin/sh should point to dash by pointing it to bash

    update-alternatives --install /bin/sh sh /bin/bash 100

    # Install useful but non-mandatory tools
    apt-get install -y emacs  git-review gdb gdbserver brctl
elif [ $DISTRIB_ID == "CentOS" ]; then
    # Standard update + upgrade dance
    yum check-update
    yum update -y
fi
