router_configure_depend =                       \
	vppinfra-install                        \
	dpdk-install                            \
	svm-install                             \
	vlib-api-install                        \
	vlib-install                            \
	vnet-install                            \
	vpp-install                             \
	netlink-install                         \
	vpp-api-test-install

router_CPPFLAGS = $(call installed_includes_fn, \
	vppinfra                                \
	dpdk                                    \
	openssl                                 \
	svm                                     \
	vlib                                    \
	vlib-api                                \
	vnet                                    \
	vpp                                     \
	netlink                                 \
	vpp-api-test)

router_LDFLAGS = $(call installed_libs_fn,      \
	vppinfra                                \
	dpdk                                    \
	openssl                                 \
	svm                                     \
	vlib                                    \
	vlib-api                                \
	vnet                                    \
	vpp                                     \
	netlink                                 \
	vpp-api-test)
