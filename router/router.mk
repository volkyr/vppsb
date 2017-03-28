+router_configure_depend =            \
+    vpp-install                      \
+    netlink-install

router_CPPFLAGS = $(call installed_includes_fn, \
	vpp                                     \
    netlink)

router_LDFLAGS = $(call installed_libs_fn,      \
	vpp                                     \
    netlink)
