# vcl-ldpreload a LD_PRELOAD library that uses VCL library for data transport

libvcl_ldpreload.so library is to be used as a LD_PRELOAD library.
User can LD_PRELOAD any application that uses POSIX socket API.
This library internally uses libvppcom.so library from VPP project.


## HowTo

The library can be compiled by running the following commands from the vppsb/vcl-ldpreload/src directory:
If VPP is installed, then 
```bash
libtoolize
aclocal
autoconf
automake 
./configure
make
sudo make install
```
If VPP is not installed, but rather built in a separate directory, you can use the VPP_DIR 'configure' argument.
```bash
autoreconf -i -f
./configure VPP_DIR=<absolute/path/to/vpp>
make
sudo make install
```bash

Useful test script can be found in VPP project:
.../vpp/test/scripts/socket_test.sh

Running socket_test.sh without parameters will give the help menu.
 
## Administrative

### Current status

This library is currently under active enhancement.

### Objective

This effort intends to be a building block for a better integration of POSIX socket applications with VPP.
It will evolve depending on the needs of the VPP community while focusing on 
LD_PRELOADing applications that use POSIX socket APIs.

### Main contributors

Shrinivasan Ganapathy - LF-ID:shganapa



