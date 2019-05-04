ACLOCAL_AMFLAGS = -I m4

AM_CPPFLAGS = -I$(top_srcdir)/include -D_GNU_SOURCE
AM_CFLAGS = $(WARN_CFLAGS)

noinst_LIBRARIES =
bin_PROGRAMS =

EXTRA_DIST = autogen.sh LICENSE README

include lib/Makemodule.am
include mkfs/Makemodule.am
include unpack/Makemodule.am
