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

##### config options #####

AC_ARG_WITH([zlib],
	[AS_HELP_STRING([--without-zlib],
			[Build without zlib compression support])],
	[case "${withval}" in
	yes) AM_CONDITIONAL([WITH_ZLIB], [true]) ;;
	no)  AM_CONDITIONAL([WITH_ZLIB], [false]) ;;
	*) AC_MSG_ERROR([bad value ${withval} for --without-zlib]) ;;
	esac],
	[AM_CONDITIONAL([WITH_ZLIB], [true])])

AC_ARG_WITH([lzma],
	[AS_HELP_STRING([--without-lzma],
			[Build without lzma compression support])],
	[case "${withval}" in
	yes) AM_CONDITIONAL([WITH_LZMA], [true]) ;;
	no)  AM_CONDITIONAL([WITH_LZMA], [false]) ;;
	*) AC_MSG_ERROR([bad value ${withval} for --without-lzma]) ;;
	esac],
	[AM_CONDITIONAL([WITH_LZMA], [true])])

##### search for dependencies #####

need_zlib="no"
need_lzma="no"

AM_COND_IF([WITH_ZLIB], [need_zlib="yes"])
AM_COND_IF([WITH_LZMA], [need_lzma="yes"])

if test "x$need_zlib" = "xyes"; then
	PKG_CHECK_MODULES(ZLIB, [zlib], [], [AC_MSG_ERROR([cannot find zlib])])
fi

if test "x$need_lzma" = "xyes"; then
	PKG_CHECK_MODULES(XZ, [liblzma >= 5.0.0], [],
			      [AC_MSG_ERROR([cannot find lzma])])
fi

##### generate output #####

AC_CONFIG_HEADERS([config.h])

AC_OUTPUT([Makefile])
