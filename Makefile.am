ACLOCAL_AMFLAGS = -I m4

AM_CPPFLAGS = -I$(top_srcdir)/include -D_GNU_SOURCE
AM_CFLAGS = $(WARN_CFLAGS)

noinst_LIBRARIES =
bin_PROGRAMS =
dist_man1_MANS =

EXTRA_DIST = autogen.sh LICENSE README

include doc/Makemodule.am
include lib/Makemodule.am
include tar/Makemodule.am
include mkfs/Makemodule.am
include unpack/Makemodule.am
