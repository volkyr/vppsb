# vcl-ldpreload a LD_PRELOAD library that uses the VPP Communications Library (VCL).

User can LD_PRELOAD any application that uses POSIX socket API.
This library internally uses libvppcom.so library from VPP project.


## HowTo

If VPP is not installed, but rather built in a separate directory, you can use the VPP_DIR 'configure' argument.
```bash
# 1. Edit vppsb/vcl-ldpreload/env.sh
## set base for directory above vpp and vppsb source

source ./env.sh

# 2. Change to VPP source directory and build

cd $VPP_DIR

# Modify uri.am to enable socket_test program

perl -pi -e 's/noinst_PROGRAMS/bin_PROGRAMS/g' $VPP_DIR/src/uri.am

# Build VPP release 

make install-dep wipe-release bootstrap dpdk-install-dev build-release

# 2. Build LD_PRELOAD library against VPP build above
## This does not install the LD_PRELOAD library in your system.
## Instead it will be referenced from the build directory set in VCL_LDPRELOAD_LIB

cd $LDP_DIR/vcl-ldpreload/src
autoreconf -i -f
./configure VPP_DIR=$VPP_DIR
make
```bash


# 3. Running the demo
## Run test script without parameters to see help menu:

cd $VPP_DIR/test/scripts
./socket_test.sh

# 4. Docker iPerf examples.
## These launch xterms. To quit, close xterms and run following docker kill cmd (WARNING: This will kill all docker containers!) 'docker kill $(docker ps -q)'


## Docker iPerf using default Linux Bridge

./socket_test.sh -bi docker-kernel

## Docker iPerf using VPP
./socket_test.sh -bi docker-preload


 

The library can be compiled by running the following commands from the vppsb/vcl-ldpreload/src directory:
If VPP is installed, then 
```bash
libtoolize
aclocal
autoconf
automake --add-missing
./configure
make
sudo make install
```

## Administrative

### Current status

This library is currently under active enhancement.

### Objective

This effort intends to be a building block for a better integration of POSIX socket applications with VPP.
It will evolve depending on the needs of the VPP community while focusing on 
LD_PRELOADing applications that use POSIX socket APIs.

### Main contributors

Shrinivasan Ganapathy - LF-ID:shganapa



