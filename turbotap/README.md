# A plugin to provide turbotap driver

This repository provides an Experimental work, a plugin to use tap
interfaces using socket API system calls "sendmmsg" or "recvmmsg"
which allows to send/receive multiple packets using one single system
call. It is a replacement for tapcli driver in VPP, which uses one
system call per packet. Hence save the time for 'context switching'
between userspace and kernel space.

The linux kernel doesn't support socket API for tap interfaces. Therefore,
a separate turbotap 'LINUX KERNEL MODULE' has been implemented to support
send and receive socket system calls.

More information can be found on the wiki page:
- http://wiki.fd.io/view/VPP_Sandbox/turbotap

Source code, build and install turbotap kernel module:
- https://github.com/vpp-dev/turbotap

## Build/Install

The turbotap driver is implemented as a plugin to send/receive packets from
kernel tap interfaces. To use it, you must BUILD and INSTALL turbotap kernel
module at first.
Then you must build plugin and put it in VPPs runtime plugin directory.
The plugin depends on vpp. This README assumes familiarity with the build
environment for both projects.

Build vpp and turbotap both at once by creating symbolic links in the
top level vpp directory to the turbotap directory as well as
symbolic links to the respective .mk files in 'build-data/packages'.

```
$ cd /git/vpp
$ ln -sf /git/vppsb/turbotap
$ ln -sf ../../turbotap/turbotap.mk build-data/packages/
```

Now build everything and create a link to the plugin in vpp's plugin path.

```
$ cd build-root
$ ./bootstrap.sh
$ make V=0 PLATFORM=vpp TAG=vpp_debug turbotap-install
$ ln -sf /git/vpp/build-root/install-vpp_debug-native/turbotap/lib64/turbotap.so.0.0.0 \
         /usr/lib/vpp_plugins/
```

Once VPP is running and the plugin is loaded, turbotap interfaces can be created or deleted.

```
$ vppctl turbotap connect turbotap0
$ vppctl turbotap delete turbotap0
```

The host operating system should see a turbotap named 'turbotap0'.

## Administrative

### Current status

Currently the turbotap driver plugin uses socket API system calls. Most of the
code is borrowed from tapcli driver in VPP. One can extend it to multi-queue driver.

### Objective

The objective of this project is to continue to build out better integration
with host operating system and for providing a basis to enable completely
or partially unmodified applications to take advantage of a fast datapath.

### Main contributors

Mohsin KAZMI  - LF-ID:sykazmi
