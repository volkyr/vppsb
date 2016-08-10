AC_INIT(turbotap, 1.0)
LT_INIT
AM_INIT_AUTOMAKE

AM_PROG_AS
AC_PROG_CC
AM_PROG_CC_C_O

AC_ARG_WITH(dpdk,
            AC_HELP_STRING([--with-dpdk],[Use the Intel dpdk]),
            [with_dpdk=1],
            [with_dpdk=0])

AC_ARG_WITH(plugin-toolkit,
            AC_HELP_STRING([--with-plugin-toolkit],
            [build using the vpp toolkit]),
            [with_plugin_toolkit=${prefix}/include],
            [with_plugin_toolkit=.])

AM_CONDITIONAL(WITH_DPDK, test "$with_dpdk" = "1")
AC_SUBST(DPDK,["-DDPDK=1"])

AC_SUBST(TOOLKIT_INCLUDE,[${with_plugin_toolkit}])
AM_CONDITIONAL(WITH_PLUGIN_TOOLKIT, test "$with_plugin_toolkit" != ".")

AC_OUTPUT([Makefile])
