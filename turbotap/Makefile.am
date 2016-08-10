AUTOMAKE_OPTIONS = foreign subdir-objects

AM_CFLAGS = -Wall -I@TOOLKIT_INCLUDE@ @DPDK@
AM_LDFLAGS = -module -shared -avoid-version

lib_LTLIBRARIES = libturbotap.la
libturbotap_la_SOURCES = turbotap/turbotap.c\
			 turbotap/device.c  \
			 turbotap/node.c   \
			 turbotap/cli.c 

nobase_include_HEADERS = turbotap/turbotap.h

if WITH_PLUGIN_TOOLKIT
install-data-hook:
	mkdir /usr/lib/vpp_plugins || true
	cp $(prefix)/lib/turbotap.so.*.*.* /usr/lib/vpp_plugins
endif
