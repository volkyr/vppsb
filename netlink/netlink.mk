netlink_configure_depend =		\
	vppinfra-install			\
	dpdk-install				\
	svm-install				\
	vlib-api-install			\
	vlib-install				\
	vnet-install				\
	vpp-install				\
	vpp-api-test-install

netlink_CPPFLAGS = $(call installed_includes_fn,	\
	vppinfra					\
	dpdk						\
	openssl						\
	svm						\
	vlib						\
	vlib-api					\
	vnet						\
	vpp						\
    vpp-api-test)

netlink_LDFLAGS = $(call installed_libs_fn,	\
	vppinfra					\
	dpdk						\
	openssl						\
	svm						\
	vlib						\
	vlib-api					\
	vnet						\
	vpp						\
	vpp-api-test)
