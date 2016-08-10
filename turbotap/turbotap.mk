turbotap_configure_depend =                       \
	vppinfra-install                        \
	dpdk-install                            \
	svm-install                             \
	vlib-api-install                        \
	vlib-install                            \
	vnet-install                            \
	vpp-install                             \
	vpp-api-test-install

turbotap_CPPFLAGS = $(call installed_includes_fn, \
	vppinfra                                \
	dpdk                                    \
	openssl                                 \
	svm                                     \
	vlib                                    \
	vlib-api                                \
	vnet                                    \
	vpp                                     \
	vpp-api-test)

turbotap_LDFLAGS = $(call installed_libs_fn,      \
	vppinfra                                \
	dpdk                                    \
	openssl                                 \
	svm                                     \
	vlib                                    \
	vlib-api                                \
	vnet                                    \
	vpp                                     \
	vpp-api-test)
