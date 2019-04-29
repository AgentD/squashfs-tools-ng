AC_PREREQ([2.60])
AC_INIT([squashfs-tools-ng], [0.1], [goliath@sigma-star.at], squashfs-tools-ng)
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([foreign dist-xz subdir-objects])
AM_SILENT_RULES([yes])
AC_PROG_CC
AC_PROG_CC_C99
AC_PROG_RANLIB
AC_PROG_INSTALL

m4_ifndef([PKG_PROG_PKG_CONFIG],
  [m4_fatal([Could not locate the pkg-config autoconf
    macros. These are usually located in /usr/share/aclocal/pkg.m4.
    If your macros are in a different location, try setting the
    environment variable AL_OPTS="-I/other/macro/dir" before running
    ./autogen.sh or autoreconf again. Make sure pkg-config is installed.])])
PKG_PROG_PKG_CONFIG

UL_WARN_ADD([-Wall])
UL_WARN_ADD([-Wextra])
UL_WARN_ADD([-Wunused])
UL_WARN_ADD([-Wmissing-prototypes])
UL_WARN_ADD([-Wmissing-declarations])
UL_WARN_ADD([-Wwrite-strings])
UL_WARN_ADD([-Wjump-misses-init])
UL_WARN_ADD([-Wuninitialized])
UL_WARN_ADD([-Winit-self])
UL_WARN_ADD([-Wlogical-op])
UL_WARN_ADD([-Wunused-but-set-parameter])
UL_WARN_ADD([-Wunused-but-set-variable])
UL_WARN_ADD([-Wunused-parameter])
UL_WARN_ADD([-Wunused-result])
UL_WARN_ADD([-Wunused-variable])
UL_WARN_ADD([-Wduplicated-cond])
UL_WARN_ADD([-Wduplicated-branches])
UL_WARN_ADD([-Wrestrict])
UL_WARN_ADD([-Wnull-dereference])
UL_WARN_ADD([-pedantic])

AC_SUBST([WARN_CFLAGS])

##### search for dependencies #####

have_zlib="no"
have_lzma="no"

PKG_CHECK_MODULES(ZLIB, [zlib], [have_zlib="yes"], [])
PKG_CHECK_MODULES(XZ, [liblzma >= 5.0.0], [have_lzma="yes"], [])

AM_CONDITIONAL([WITH_ZLIB], [test "x$have_zlib" == "xyes"])
AM_CONDITIONAL([WITH_LZMA], [test "x$have_lzma" == "xyes"])

##### generate output #####

AC_CONFIG_HEADERS([config.h])

AC_OUTPUT([Makefile])
