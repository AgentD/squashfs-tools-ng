AC_PREREQ([2.60])
AC_INIT([squashfs-tools-ng], [0.7], [goliath@infraroot.at], squashfs-tools-ng)
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([foreign dist-xz subdir-objects])
AM_SILENT_RULES([yes])
LT_INIT([win32-dll])
AC_PROG_CC
AC_PROG_CC_C99
AC_PROG_INSTALL
AC_SYS_LARGEFILE
AC_CANONICAL_HOST

m4_ifndef([PKG_PROG_PKG_CONFIG],
  [m4_fatal([Could not locate the pkg-config autoconf
    macros. These are usually located in /usr/share/aclocal/pkg.m4.
    If your macros are in a different location, try setting the
    environment variable AL_OPTS="-I/other/macro/dir" before running
    ./autogen.sh or autoreconf again. Make sure pkg-config is installed.])])
PKG_PROG_PKG_CONFIG
PKG_INSTALLDIR

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

##### target detection & target specific stuff #####

build_windows="no"

case "${host_os}" in
cygwin*|mingw*)
	build_windows=yes
	;;
esac

AM_CONDITIONAL([WINDOWS], [test "x$build_windows" = "xyes"])

##### config options #####

AC_ARG_WITH([xz],
	[AS_HELP_STRING([--with-xz], [Build with xz compression support])],
	[want_xz="${withval}"], [want_xz="maybe"])

AC_ARG_WITH([lzo],
	[AS_HELP_STRING([--with-lzo], [Build with lzo compression support])],
	[want_lzo="${withval}"], [want_lzo="maybe"])

AC_ARG_WITH([lz4],
	[AS_HELP_STRING([--without-lz4], [Build with lz4 compression support])],
	[want_lz4="${withval}"], [want_lz4="maybe"])

AC_ARG_WITH([zstd],
	[AS_HELP_STRING([--with-zstd], [Build with zstd compression support])],
	[want_zstd="${withval}"], [want_zstd="maybe"])

AC_ARG_WITH([selinux],
	[AS_HELP_STRING([--with-selinux],
			[Build with SELinux label file support])],
	[want_selinux="${withval}"], [want_selinux="maybe"])

AC_ARG_WITH([pthread],
	[AS_HELP_STRING([--without-pthread],
			[Build without pthread based block compressor])],
	[want_pthread="${withval}"], [want_pthread="yes"])

AC_ARG_WITH([tools],
	[AS_HELP_STRING([--without-tools],
			[Only build libsquashfs, do not build the tools.])],
	[build_tools="${withval}"], [build_tools="yes"])

if test "x$build_windows" = "xyes"; then
	want_pthread="no"
fi

if test "x$build_windows" = "xyes"; then
	build_tools="no"
fi

AM_CONDITIONAL([BUILD_TOOLS], [test "x$build_tools" = "xyes"])

##### Doxygen reference manual #####

AC_CHECK_PROGS([DOXYGEN], [doxygen])
AM_CONDITIONAL([HAVE_DOXYGEN], [test -n "$DOXYGEN"])

if test -z "$DOXYGEN"; then
	AC_MSG_WARN([Doxygen not found - continuing without Doxygen support])
else
	AC_CONFIG_FILES([Doxyfile])

	DX_DOXYGEN_FEATURE(ON)
	DX_DOT_FEATURE(ON)
	DX_HTML_FEATURE(ON)
	DX_CHM_FEATURE(OFF)
	DX_CHI_FEATURE(OFF)
	DX_MAN_FEATURE(OFF)
	DX_RTF_FEATURE(OFF)
	DX_XML_FEATURE(OFF)
	DX_PDF_FEATURE(OFF)
	DX_PS_FEATURE(OFF)
	DX_INIT_DOXYGEN(libsquashfs, Doxyfile)
fi

##### search for dependencies #####

AM_CONDITIONAL([WITH_GZIP], [true])
PKG_CHECK_MODULES(ZLIB, [zlib], [], [AC_MSG_ERROR([cannot find zlib])])

PKG_CHECK_MODULES(XZ, [liblzma >= 5.0.0],
		      [AM_CONDITIONAL([WITH_XZ], [true])],
		      [AM_CONDITIONAL([WITH_XZ], [false])])

PKG_CHECK_MODULES(LZ4, [liblz4],
		       [AM_CONDITIONAL([WITH_LZ4], [true])],
		       [AM_CONDITIONAL([WITH_LZ4], [false])])

PKG_CHECK_MODULES(ZSTD, [libzstd],
			[AM_CONDITIONAL([WITH_ZSTD], [true])],
			[AM_CONDITIONAL([WITH_ZSTD], [false])])

libsqfs_dep_mod=""
AM_COND_IF([WITH_GZIP], [libsqfs_dep_mod="$libsqfs_dep_mod zlib"], [])
AM_COND_IF([WITH_XZ], [libsqfs_dep_mod="$libsqfs_dep_mod liblzma >= 5.0.0"], [])
AM_COND_IF([WITH_LZ4], [libsqfs_dep_mod="$libsqfs_dep_mod liblz4"], [])
AM_COND_IF([WITH_ZSTD], [libsqfs_dep_mod="$libsqfs_dep_mod libzstd"], [])
AC_SUBST([LIBSQFS_DEP_MOD], ["$libsqfs_dep_mod"])

AM_CONDITIONAL([WITH_LZO], [false])

if test "x$want_lzo" != "xno"; then
	AC_ARG_VAR([LZO_CFLAGS], [C compiler flags for lzo])
	AC_ARG_VAR([LZO_LIBS], [linker flags for lzo])
	AM_CONDITIONAL([WITH_LZO], [true])

	AC_CHECK_LIB([lzo2], [lzo1x_1_15_compress], [LZO_LIBS="-llzo2"],
			     [AC_CHECK_LIB([lzo],[lzo1x_1_15_compress],
				[LZO_LIBS="-llzo"],
			     	[AM_CONDITIONAL([WITH_LZO], [false])]
			     )]
		    )
fi

if test "x$build_tools" = "xyes"; then
	AM_CONDITIONAL([WITH_SELINUX], [false])

	if test "x$want_selinux" != "xno"; then
		AM_CONDITIONAL([WITH_SELINUX], [true])

		PKG_CHECK_MODULES(LIBSELINUX, [libselinux], [],
				  [AM_CONDITIONAL([WITH_SELINUX], [false])])
		AC_CHECK_HEADERS([selinux/selinux.h], [],
				 [AM_CONDITIONAL([WITH_SELINUX], [false])])
		AC_CHECK_HEADERS([selinux/label.h], [],
				 [AM_CONDITIONAL([WITH_SELINUX], [false])])
	fi
else
	want_selinux="no"
fi

case "$want_xz" in
yes) AM_COND_IF([WITH_XZ], [], [AC_MSG_ERROR([cannot find xz sdk])]) ;;
no)  AM_CONDITIONAL([WITH_XZ], [false]) ;;
esac

case "$want_lzo" in
yes) AM_COND_IF([WITH_LZO], [], [AC_MSG_ERROR([cannot find lzo library])]) ;;
no)  AM_CONDITIONAL([WITH_LZO], [false]) ;;
esac

case "$want_lz4" in
yes) AM_COND_IF([WITH_LZ4], [], [AC_MSG_ERROR([cannot find lz4 library])]) ;;
no)  AM_CONDITIONAL([WITH_LZ4], [false]) ;;
esac

case "$want_zstd" in
yes) AM_COND_IF([WITH_ZSTD], [], [AC_MSG_ERROR([cannot find zstd library])]) ;;
no)  AM_CONDITIONAL([WITH_ZSTD], [false]) ;;
esac

case "$want_selinux" in
yes) AM_COND_IF([WITH_SELINUX], [], [AC_MSG_ERROR([cannot find selinux])]) ;;
no)  AM_CONDITIONAL([WITH_SELINUX], [false]) ;;
esac

AM_CONDITIONAL([HAVE_PTHREAD], [false])
if test "x$want_pthread" = "xyes"; then
	AX_PTHREAD([AM_CONDITIONAL([HAVE_PTHREAD], [true])],
		   [AC_MSG_ERROR([cannot find pthread])])
fi

##### additional checks #####
AX_COMPILE_CHECK_SIZEOF(time_t)
AX_COMPILE_CHECK_SIZEOF(size_t)
AX_COMPILE_CHECK_SIZEOF(short)
AX_COMPILE_CHECK_SIZEOF(int)
AX_COMPILE_CHECK_SIZEOF(long)
AX_COMPILE_CHECK_SIZEOF(long long)

AC_CHECK_HEADERS([sys/xattr.h], [], [])
AC_CHECK_HEADERS([sys/sysinfo.h], [], [])

AC_CHECK_FUNCS([strndup getline getsubopt])

##### generate output #####

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([lib/sqfs/libsquashfs.pc])

AC_OUTPUT([Makefile])
