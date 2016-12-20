# Vhost and VPP testing script

This script intends to provide a reference script for testing [VPP](http://fd.io/) vhost-user performances.
It uses the basic topology PHY -> VM -> PHY.
You will therefore need a machine with two physical interfaces and the ability to run a VM with qemu in
order to run the test.

## A word about the VM

The VM that is executed is a CLONE of the running system.
In practice, that means that the VM is using, as root filesystem,
a mounted OverlayFS instance of the root directory (Meaning that
the VM will share the same file, but all updates will not affect the
root file system).

This also means that the VM boots on a shared file system, which implies
that the initramfs image must include the 9p driver.

One way of doing it is documented [here] (http://unix.stackexchange.com/questions/90423/can-virtfs-9p-be-used-as-root-file-system).
$ printf '%s\n' 9p 9pnet 9pnet_virtio | sudo tee -a /etc/initramfs-tools/modules
$ sudo update-initramfs -u


## HowTo

$ cd vhost-test

First copy and update the configuration file such that the parameters
correspond to your setup.

$ cp conf.sh.default conf.sh
$ vim conf.sh

Then, run the setup.

$ ./vhost.sh start

Once the setup is running, you can re-pin the processes.
This is important, as it also performs 'chrt' on the working threads.

$ ./vhost.sh pin

Once the setup is running, you can:
- Log into the VM
$ ./vhost.sh ssh

- Log into VPP
$ sudo screen -r vhtestvpp


Finally, when you are done, you can stop the VMs.

$ ./vhost.sh stop

## Traffic Generation with MoonGen

mg.lua is intended to be used with the MoonGen packet generator.
This script measures packet loss and forwarding with a user-defined
granularity. The script keeps running while consecutive measures are
too far on both Tx axis (--maxRateInterval option) or on the drop rate
axis (--targetDropRateRatio). The latter is a ratio instead of an interval,
as the packet drop is mostly a logarithmic value.

The script is used as follows:
sudo /path/to/MoonGen ./mg.lua <options>

Mandatory options are:
--rxport <id> MoonGen's rx port index
--txport <id> MoonGen's tx port index
--dst <hwaddr> Frame's destination L2 address

Other options are:
--duration <seconds> Each measurement duration
--frameSize <bytes> Each frame size
--maxRateInterval <%> Max Tx interval between measure
--targetDropRateRatio <ratio> Max ratio between two drop rate measures
--minRateInterval <%> Min Tx interval (Will override drop ratio in case of non-continuous)
--out <file> Output file

## Administrativa

### Current status

This script hasn't been tested by anyone but me for now.
You should therefore expect bugs on different setups.

### Main contributors

Pierre Pfister - LF-ID:ppfister


