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

AC_ARG_WITH([gzip],
	[AS_HELP_STRING([--without-gzip],
			[Build without gzip compression support])],
	[case "${withval}" in
	yes) AM_CONDITIONAL([WITH_GZIP], [true]) ;;
	no)  AM_CONDITIONAL([WITH_GZIP], [false]) ;;
	*) AC_MSG_ERROR([bad value ${withval} for --without-gzip]) ;;
	esac],
	[AM_CONDITIONAL([WITH_GZIP], [true])])

AC_ARG_WITH([xz],
	[AS_HELP_STRING([--without-xz],
			[Build without xz compression support])],
	[case "${withval}" in
	yes) AM_CONDITIONAL([WITH_XZ], [true]) ;;
	no)  AM_CONDITIONAL([WITH_XZ], [false]) ;;
	*) AC_MSG_ERROR([bad value ${withval} for --without-xz]) ;;
	esac],
	[AM_CONDITIONAL([WITH_XZ], [true])])

AC_ARG_WITH([lzo],
	[AS_HELP_STRING([--without-lzo],
			[Build without lzo compression support])],
	[case "${withval}" in
	yes) AM_CONDITIONAL([WITH_LZO], [true]) ;;
	no)  AM_CONDITIONAL([WITH_LZO], [false]) ;;
	*) AC_MSG_ERROR([bad value ${withval} for --without-lzo]) ;;
	esac],
	[AM_CONDITIONAL([WITH_LZO], [true])])

AC_ARG_WITH([lz4],
	[AS_HELP_STRING([--without-lz4],
			[Build without lz4 compression support])],
	[case "${withval}" in
	yes) AM_CONDITIONAL([WITH_LZ4], [true]) ;;
	no)  AM_CONDITIONAL([WITH_LZ4], [false]) ;;
	*) AC_MSG_ERROR([bad value ${withval} for --without-lz4]) ;;
	esac],
	[AM_CONDITIONAL([WITH_LZ4], [true])])

AC_ARG_WITH([selinux],
	[AS_HELP_STRING([--without-selinux],
			[Build without SELinux label file support])],
	[case "${withval}" in
	yes) AM_CONDITIONAL([WITH_SELINUX], [true]) ;;
	no)  AM_CONDITIONAL([WITH_SELINUX], [false]) ;;
	*) AC_MSG_ERROR([bad value ${withval} for --without-selinux]) ;;
	esac],
	[AM_CONDITIONAL([WITH_SELINUX], [true])])

##### search for dependencies #####

need_zlib="no"
need_xz="no"
need_lzo="no"
need_lz4="no"
need_selinux="no"

AM_COND_IF([WITH_GZIP], [need_zlib="yes"])
AM_COND_IF([WITH_XZ], [need_xz="yes"])
AM_COND_IF([WITH_LZO], [need_lzo="yes"])
AM_COND_IF([WITH_LZ4], [need_lz4="yes"])
AM_COND_IF([WITH_SELINUX], [need_selinux="yes"])

if test "x$need_zlib" = "xyes"; then
	PKG_CHECK_MODULES(ZLIB, [zlib], [], [AC_MSG_ERROR([cannot find zlib])])
fi

if test "x$need_xz" = "xyes"; then
	PKG_CHECK_MODULES(XZ, [liblzma >= 5.0.0], [],
			      [AC_MSG_ERROR([cannot find xz sdk])])
fi

if test "x$need_lzo" = "xyes"; then
	AC_ARG_VAR([LZO_CFLAGS], [C compiler flags for lzo])
	AC_ARG_VAR([LZO_LIBS], [linker flags for lzo])
	AC_CHECK_LIB([lzo2], [lzo1x_1_15_compress], [LZO_LIBS="-llzo2"],
		[AC_CHECK_LIB([lzo],[lzo1x_1_15_compress],[LZO_LIBS="-llzo"],
			[AC_MSG_ERROR([cannot find lzo library])]
		)]
	)
fi

if test "x$need_lz4" = "xyes"; then
	PKG_CHECK_MODULES(LZ4, [liblz4], [],
			       [AC_MSG_ERROR([cannot lz4 library])])
fi

if test "x$need_selinux" = "xyes"; then
	PKG_CHECK_MODULES(LIBSELINUX, [libselinux], [],
				      [AC_MSG_ERROR([cannot find libselinux])])

	AC_CHECK_HEADERS([selinux/selinux.h], [],
			 [AC_MSG_ERROR([cannot find SELinux system headers])])
	AC_CHECK_HEADERS([selinux/label.h], [],
			 [AC_MSG_ERROR([cannot find SELinux system headers])])
fi

##### generate output #####

AC_CONFIG_HEADERS([config.h])

AC_OUTPUT([Makefile])
