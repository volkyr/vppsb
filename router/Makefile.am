AUTOMAKE_OPTIONS = foreign subdir-objects

AM_CFLAGS = -Wall -I@TOOLKIT_INCLUDE@

lib_LTLIBRARIES = router.la
router_la_SOURCES =     \
    router/tap_inject.c \
    router/tap_inject_netlink.c \
    router/tap_inject_node.c \
    router/tap_inject_tap.c

nobase_include_HEADERS =	\
    router/tap_inject.h

router_la_LDFLAGS = -module
router_la_LIBADD = -lrtnl

if WITH_PLUGIN_TOOLKIT
install-data-hook:
	mkdir /usr/lib/vpp_plugins || true
	cp $(prefix)/lib/router.so.*.*.* /usr/lib/vpp_plugins
endif
