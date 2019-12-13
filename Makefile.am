ACLOCAL_AMFLAGS = -I m4

AM_CPPFLAGS = -I$(top_srcdir)/include -D_GNU_SOURCE
AM_CFLAGS = $(WARN_CFLAGS)

if WITH_LZO
AM_CPPFLAGS += -DWITH_LZO
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

EXTRA_DIST = autogen.sh README.md CHANGELOG.md COPYING.md mkwinbins.sh licenses
TESTS =

include lib/sqfs/Makemodule.am

if BUILD_TOOLS
include doc/Makemodule.am
include lib/fstree/Makemodule.am
include lib/common/Makemodule.am
include lib/tar/Makemodule.am
include lib/compat/Makemodule.am
include tar/Makemodule.am
include mkfs/Makemodule.am
include unpack/Makemodule.am
include difftool/Makemodule.am
endif

include tests/Makemodule.am

if HAVE_DOXYGEN
@DX_RULES@

MOSTLYCLEANFILES = $(DX_CLEANFILES)
endif
