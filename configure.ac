AC_PREREQ([2.60])
AC_INIT([squashfs-tools-ng], [1.1.2], [goliath@infraroot.at], squashfs-tools-ng)
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([foreign dist-xz subdir-objects])
AM_SILENT_RULES([yes])
AC_PROG_CC([cc gcc clang])
LT_INIT([win32-dll])
AC_PROG_CC_C99
AC_PROG_INSTALL
AC_PROG_SED
AC_SYS_LARGEFILE
AC_CANONICAL_HOST

AC_SUBST([LIBSQUASHFS_SO_VERSION], [3:1:2])

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
UL_WARN_ADD([-Wpedantic])
UL_WARN_ADD([-Walloc-zero])
UL_WARN_ADD([-Wcast-align])
UL_WARN_ADD([-Wcast-align=strict])
UL_WARN_ADD([-Wcast-qual])
UL_WARN_ADD([-Wdangling-else])
UL_WARN_ADD([-Wdate-time])
UL_WARN_ADD([-Wdouble-promotion])
UL_WARN_ADD([-Wformat-nonliteral])
UL_WARN_ADD([-Wformat-security])
UL_WARN_ADD([-Wformat-signedness])
UL_WARN_ADD([-Wmissing-format-attribute])
UL_WARN_ADD([-Wmissing-noreturn])
UL_WARN_ADD([-Wpacked])
UL_WARN_ADD([-Wpacked-not-aligned])
UL_WARN_ADD([-Wredundant-decls])
UL_WARN_ADD([-Wswitch-default])
UL_WARN_ADD([-Wswitch-enum])
UL_WARN_ADD([-Wtrampolines])
UL_WARN_ADD([-Wundef])
UL_WARN_ADD([-Wunused-const-variable])
UL_WARN_ADD([-Wvla])

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

AC_ARG_WITH([bzip2],
	[AS_HELP_STRING([--with-bzip2],
			[Build with bzip2 compression support])],
	[], [with_bzip2="check"])

AC_ARG_WITH([xz],
	[AS_HELP_STRING([--with-xz], [Build with xz compression support])],
	[], [with_xz="check"])

AC_ARG_WITH([lzo],
	[AS_HELP_STRING([--with-lzo], [Build with lzo compression support])],
	[], [with_lzo="check"])

AC_ARG_WITH([lz4],
	[AS_HELP_STRING([--with-lz4], [Build with lz4 compression support])],
	[], [with_lz4="check"])

AC_ARG_WITH([builtin-lz4],
	[AS_HELP_STRING([--with-builtin-lz4], [Use a custom, static liblz4])],
	[], [with_builtin_lz4="no"])

AC_ARG_WITH([zstd],
	[AS_HELP_STRING([--with-zstd], [Build with zstd compression support])],
	[], [with_zstd="check"])

AC_ARG_WITH([gzip],
	[AS_HELP_STRING([--with-gzip], [Build with zlib compression support])],
	[], [with_gzip="check"])

AC_ARG_WITH([builtin-zlib],
	[AS_HELP_STRING([--with-builtin-zlib], [Use a custom, static zlib])],
	[], [with_builtin_zlib="no"])

AC_ARG_WITH([selinux],
	[AS_HELP_STRING([--with-selinux],
			[Build with SELinux label file support])],
	[], [with_selinux="check"])

AC_ARG_WITH([pthread],
	[AS_HELP_STRING([--without-pthread],
			[Build without pthread based block compressor])],
	[], [with_pthread="yes"])

AC_ARG_WITH([tools],
	[AS_HELP_STRING([--without-tools],
			[Only build libsquashfs, do not build the tools.])],
	[], [with_tools="yes"])

AC_ARG_ENABLE([custom-alloc],
	[AS_HELP_STRING([--disable-custom-alloc],
			[Do not used any custom allocators.])],
	[], [enable_custom_alloc="yes"])

AC_ARG_ENABLE([corpora-tests],
	[AS_HELP_STRING([--enable-corpora-tests],
			[Perform corpora based reproducability tests.])],
	[], [enable_corpora_tests="no"])

AM_CONDITIONAL([CORPORA_TESTS], [test "x$enable_corpora_tests" = "xyes"])

AS_IF([test "x$build_windows" = "xyes"], [with_pthread="no"], [])
AS_IF([test "x$with_tools" != "xyes"], [with_selinux="no"], [])
AM_CONDITIONAL([BUILD_TOOLS], [test "x$with_tools" = "xyes"])

##### Doxygen reference manual #####

AC_CHECK_PROGS([DOXYGEN], [doxygen])
AM_CONDITIONAL([HAVE_DOXYGEN], [test -n "$DOXYGEN"])

if test -z "$DOXYGEN"; then
	with_doxygen="no"
else
	with_doxygen="yes"

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

AC_ARG_VAR([BZIP2_CFLAGS], [C compiler flags for lib bzip2])
AC_ARG_VAR([BZIP2_LIBS], [linker flags for lib bzip2])

AS_IF([test -z "$BZIP2_LIBS" -a "x$with_bzip2" != "xno"], [
	AC_CHECK_LIB([bz2], [BZ2_bzCompress],
		     [AC_CHECK_HEADERS([bzlib.h], [BZIP2_LIBS="-lbz2"], [])],
		     [])
], [])

AS_IF([test -z "$BZIP2_LIBS"], [AS_IF([test "x$with_bzip2" = "xyes"],
				    [AC_MSG_ERROR([cannot find Bzip2 libs])],
				    [with_bzip2="no"])],
			       [with_bzip2="yes"])

AS_IF([test "x$with_gzip" != "xno" -a "x$with_builtin_zlib" != "xyes"], [
	PKG_CHECK_MODULES(ZLIB, [zlib], [with_gzip="yes"],
				[AS_IF([test "x$with_gzip" != "xcheck"],
				       [AC_MSG_ERROR([cannot find zlib])],
				       [with_gzip="no"])])
], [])

AS_IF([test "x$with_builtin_zlib" != "xno"], [with_gzip="yes"], [])

AS_IF([test "x$with_xz" != "xno"], [
	PKG_CHECK_MODULES(XZ, [liblzma >= 5.0.0], [with_xz="yes"],
			      [AS_IF([test "x$with_xz" != "xcheck"],
				     [AC_MSG_ERROR([cannot XZ sdk])],
				     [with_xz="no"])])
], [])

AS_IF([test "x$with_lz4" != "xno" -a "x$with_builtin_lz4" != "xyes"], [
	PKG_CHECK_MODULES(LZ4, [liblz4], [with_lz4="yes"],
			       [AS_IF([test "x$with_lz4" = "xyes"],
				      [AC_MSG_ERROR([cannot find liblz4])],
				      [with_lz4="no"])])
], [])

AS_IF([test "x$with_builtin_lz4" != "xno"], [with_lz4="yes"], [])

AS_IF([test "x$with_zstd" != "xno"], [
	PKG_CHECK_MODULES(ZSTD, [libzstd >= 1.3.1], [with_zstd="yes"],
				[AS_IF([test "x$with_zstd" = "xyes"],
				       [AC_MSG_ERROR([cannot find zstd])],
				       [with_zstd="no"])])
], [])

AC_TEST_ZSTD_STREAM

AS_IF([test "x$with_selinux" != "xno"], [
	have_selinux="yes"

	PKG_CHECK_MODULES(LIBSELINUX, [libselinux], [], [have_selinux="no"])
	AC_CHECK_HEADERS([selinux/selinux.h], [], [have_selinux="no"])
	AC_CHECK_HEADERS([selinux/label.h], [], [have_selinux="no"])

	AS_IF([test "x$with_selinux" != "xcheck" -a "x$have_selinux" = "xno"],
	      [AC_MSG_ERROR([cannot find SELinux libraries])],
	      [with_selinux="$have_selinux"])
], [])

AC_ARG_VAR([LZO_CFLAGS], [C compiler flags for lzo])
AC_ARG_VAR([LZO_LIBS], [linker flags for lzo])

AS_IF([test -z "$LZO_LIBS" -a "x$with_lzo" != "xno"], [
	AC_CHECK_LIB([lzo2], [lzo1x_1_15_compress], [LZO_LIBS="-llzo2"],
			     [AC_CHECK_LIB([lzo],[lzo1x_1_15_compress],
				[LZO_LIBS="-llzo"],
			     	[]
			     )]
		    )
], [])

AS_IF([test -z "$LZO_LIBS"], [AS_IF([test "x$with_lzo" = "xyes"],
				    [AC_MSG_ERROR([cannot find liblzo])],
				    [with_lzo="no"])],
			     [with_lzo="yes"])

AS_IF([test "x$with_pthread" != "xno"], [
	AX_PTHREAD([with_pthread="yes"],
		   [AS_IF([test "x$with_pthread" = "xyes"],
			  [AC_MSG_ERROR([cannot find pthread])],
			  [with_pthread="no"])])
], [])

AM_CONDITIONAL([WITH_BZIP2], [test "x$with_bzip2" = "xyes"])
AM_CONDITIONAL([WITH_GZIP], [test "x$with_gzip" = "xyes"])
AM_CONDITIONAL([WITH_XZ], [test "x$with_xz" = "xyes"])
AM_CONDITIONAL([WITH_LZ4], [test "x$with_lz4" = "xyes"])
AM_CONDITIONAL([WITH_ZSTD], [test "x$with_zstd" = "xyes"])
AM_CONDITIONAL([WITH_LZO], [test "x$with_lzo" = "xyes"])
AM_CONDITIONAL([WITH_SELINUX], [test "x$with_selinux" = "xyes"])
AM_CONDITIONAL([HAVE_PTHREAD], [test "x$with_pthread" = "xyes"])

AM_CONDITIONAL([WITH_OWN_LZ4], [test "x$with_builtin_lz4" = "xyes"])
AM_CONDITIONAL([WITH_OWN_ZLIB], [test "x$with_builtin_zlib" = "xyes"])

libsqfs_dep_mod=""
AS_IF([test "x$with_lz4" = "xyes" -a "x$with_builtin_lz4" != "xyes"],
	[libsqfs_dep_mod="$libsqfs_dep_mod liblz4"], [])

AS_IF([test "x$with_gzip" = "xyes" -a "x$with_builtin_zlib" != "xyes"],
	[libsqfs_dep_mod="$libsqfs_dep_mod zlib"], [])

AM_COND_IF([WITH_XZ], [libsqfs_dep_mod="$libsqfs_dep_mod liblzma >= 5.0.0"], [])
AM_COND_IF([WITH_ZSTD], [libsqfs_dep_mod="$libsqfs_dep_mod libzstd"], [])
AC_SUBST([LIBSQFS_DEP_MOD], ["$libsqfs_dep_mod"])

PKG_CHECK_MODULES(READLINE, [readline], [have_readline="yes"],
			    [have_readline="no"])
AM_CONDITIONAL([WITH_READLINE], [test "x$have_readline" = "xyes"])

##### additional checks #####
AC_CHECK_HEADERS([sys/xattr.h], [], [])
AC_CHECK_HEADERS([sys/sysinfo.h], [], [])
AC_CHECK_HEADERS([alloca.h], [], [])

AC_CHECK_FUNCS([strndup getopt getopt_long getsubopt fnmatch])

##### generate output #####

AM_CONDITIONAL([CUSTOM_ALLOC], [test "x$enable_custom_alloc" = "xyes"])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([lib/sqfs/libsquashfs1.pc])
AC_CONFIG_FILES([Doxyfile])
AC_CONFIG_FILES([tests/cantrbry.sh], [chmod +x tests/cantrbry.sh])
AC_CONFIG_FILES([tests/test_tar_sqfs.sh], [chmod +x tests/test_tar_sqfs.sh])
AC_CONFIG_FILES([tests/pack_dir_root.sh], [chmod +x tests/pack_dir_root.sh])
AC_CONFIG_FILES([tests/tarcompress.sh], [chmod +x tests/tarcompress.sh])

AC_OUTPUT([Makefile])

AC_MSG_RESULT([
	${PACKAGE}  ${VERSION}

	prefix:            ${prefix}
	exec prefix:       ${exec_prefix}

	runstatedir:       ${runstatedir}
	bindir:            ${bindir}
	sbindir:           ${sbindir}
	libdir:            ${libdir}
	includedir:        ${includedir}

	compiler:          ${CC}
	cflags:            ${CFLAGS}
	ldflags:           ${LDFLAGS}

	GZIP support:      ${with_gzip}
	XZ/LZMA support:   ${with_xz}
	LZO support:       ${with_lzo}
	LZ4 support:       ${with_lz4}
	ZSTD support:      ${with_zstd}
	BZIP2 support:     ${with_bzip2}

	SELinux support:   ${with_selinux}
	Using pthreads:    ${with_pthread}

	Custom allocators: ${enable_custom_alloc}

	Building tools:    ${with_tools}
	Doxygen found:     ${with_doxygen}

	warnings:

${WARN_CFLAGS}

	Type 'make' or 'make <utilname>' to compile.
])
