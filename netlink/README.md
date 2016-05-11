# Netlink library for VPP plugins

This VPP package provides a rtnetlink-based library to be used by [VPP](http://fd.io/) plugins and other extensions.

## HowTo

The library and test plugin can be compiled by copying netlink.mk into VPP's build tree build-data/packages/ directory and using VPP make target netlink-install.

Note that VPP build system can be configured to look in other directories for additional build-data/packages/ directories by adding those to build-root/build-config.mk SOURCE_PATH variable.

## Administrativa

### Current status

This library is still under active development, which means the defined headers and functions are subject to changes without notice.

### Objective

This effort intends to be a building block for a better integration of VPP into Linux.
It will evolve depending on the needs of the VPP community while focusing on the relations between VPP plugins and the Linux networking stack.

### Main contributors

Pierre Pfister - LF-ID:ppfister


