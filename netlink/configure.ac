AC_INIT(netlink, 1.0)
LT_INIT
AM_INIT_AUTOMAKE

AM_PROG_AS
AC_PROG_CC
AM_PROG_CC_C_O

AC_ARG_VAR(VPP_DIR,[ vpp build directory ])
AM_CONDITIONAL([VPP_DIR_SET], [test ! -z "$VPP_DIR"])

AC_ARG_ENABLE([debug],
[  --enable-debug    Turn on debugging],
 [if test x$enableval = xyes; then
    AC_DEFINE(DEBUG, 1, [Define this to enable debug.])
    debug=true
 fi], [debug=false])
AM_CONDITIONAL([DEBUG], [test x$debug = xtrue])

AC_OUTPUT([Makefile])
