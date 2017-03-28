netlink_configure_depend = vpp-install

netlink_CPPFLAGS = $(call installed_includes_fn, vpp)

netlink_LDFLAGS = $(call installed_libs_fn, vpp)
