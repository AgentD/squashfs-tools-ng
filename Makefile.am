ACLOCAL_AMFLAGS = -I m4

AM_CPPFLAGS = -I$(top_srcdir)/include -D_GNU_SOURCE
AM_CFLAGS = $(WARN_CFLAGS)

if WITH_LZO
AM_CPPFLAGS += -DWITH_LZO
endif

if CUSTOM_ALLOC
else
AM_CPPFLAGS += -DNO_CUSTOM_ALLOC
endif

noinst_LTLIBRARIES =
noinst_LIBRARIES =
noinst_PROGRAMS =
bin_PROGRAMS =
lib_LTLIBRARIES =
dist_man1_MANS =
check_PROGRAMS =
check_SCRIPTS =
pkgconfig_DATA =

EXTRA_DIST = autogen.sh README.md CHANGELOG.md COPYING.md licenses doc
TESTS =

include lib/sqfs/Makemodule.am
include lib/util/Makemodule.am
include lib/xfrm/Makemodule.am

include lib/fstree/Makemodule.am
include lib/common/Makemodule.am
include lib/tar/Makemodule.am
include lib/compat/Makemodule.am
include bin/gensquashfs/Makemodule.am
include bin/rdsquashfs/Makemodule.am
include bin/sqfs2tar/Makemodule.am
include bin/sqfsdiff/Makemodule.am
include bin/tar2sqfs/Makemodule.am

include extras/Makemodule.am

if HAVE_DOXYGEN
@DX_RULES@

MOSTLYCLEANFILES = $(DX_CLEANFILES)
endif
