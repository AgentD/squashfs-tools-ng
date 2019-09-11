ACLOCAL_AMFLAGS = -I m4

AM_CPPFLAGS = -I$(top_srcdir)/include -D_GNU_SOURCE
AM_CFLAGS = $(WARN_CFLAGS)

noinst_LTLIBRARIES =
noinst_LIBRARIES =
noinst_PROGRAMS =
bin_PROGRAMS =
lib_LTLIBRARIES =
dist_man1_MANS =
check_PROGRAMS =

EXTRA_DIST = autogen.sh LICENSE-gpl.txt LICENSE-lgpl.txt README.md CHANGELOG.md
TESTS =

include doc/Makemodule.am
include lib/fstree/Makemodule.am
include lib/sqfs/Makemodule.am
include lib/sqfshelper/Makemodule.am
include lib/tar/Makemodule.am
include lib/util/Makemodule.am
include tar/Makemodule.am
include mkfs/Makemodule.am
include unpack/Makemodule.am
include difftool/Makemodule.am
include tests/Makemodule.am

if HAVE_DOXYGEN
@DX_RULES@

MOSTLYCLEANFILES = $(DX_CLEANFILES)
endif
