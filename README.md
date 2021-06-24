# About

SquashFS is a highly compressed, read only file system often used as a root fs
on embedded devices, live systems or simply as a compressed archive format.

Think of it as a .tar.gz that you can mount (or XZ, LZO, LZ4, ZSTD).

This project originally started out as a fork of squashfs-tools 4.3, after
encountering some short comings and realizing that there have been no updates
on the SourceForge site or mailing list for a long time. Even before the first
public release, the fork was replaced with a complete re-write after growing
frustrated with the existing code base. For lack of a better name, and because
the original appeared to be unmaintained at the time, the name squashfs-tools-ng
was kept, although the published code base technically never had any connection
to squashfs-tools.

Maintenance of the original squashfs-tools has since resumed, squashfs-tools
version 4.4 was released and continues to be maintained in parallel. The
utilities provided by squashfs-tools-ng offer alternative tooling and are
intentionally named differently, so both packages can be installed side by
side.

The actual guts of squashfs-tools-ng are encapsulated in a library with a
generic API designed to make SquashFS available to other applications as an
embeddable, extensible archive format (or to simply read, write or manipulate
SquashFS file systems).

The utility programs are largely command line wrappers around the library. The
following tools are provided:

 - `gensquashfs` can be used to produce SquashFS images from `gen_init_cpio`
   like file listings or simply pack an input directory. Can use an SELinux
   contexts file (see selabel_file(5)) to generate SELinux labels.
 - `rdsquashfs` can be used to inspect and unpack SquashFS images.
 - `sqfs2tar` can turn a SquashFS image into a tarball, written to stdout.
 - `tar2sqfs` can turn a tarball (read from stdin) into a SquashFS image.
 - `sqfsdiff` can compare the contents of two SquashFS images.

The library and the tools that produce SquashFS images are designed to operate
deterministically. Same input will produce byte-for-byte identical
output. Failure to do so is treated as a critical bug.

# Installing

A number of Linux distributions already offer squashfs-tools-ng through their
package management system. Replogy maintains an up to date list:

[![Packaging status](https://repology.org/badge/vertical-allrepos/squashfs-tools-ng.svg)](https://repology.org/project/squashfs-tools-ng/versions)

## Pre-built Windows binary Packages

Pre-compiled binary packages for Windows are available here:

https://infraroot.at/pub/squashfs/windows

Those packages contain the binaries for the tools, the SquashFS library
and pre-compiled dependency libraries (zstd, lzo, lzma; others are built in).

The binary package does not contain any source code. See below on how to obtain
and compile the source for squashfs-tools-ng. The corresponding source code
from which the 3rd party libraries have been built is also available for
download at the above location.

The headers and import libraries to build applications that use libsquashfs are
included. For convenience, the pre-compiled, 3rd party dependency libraries
also come with headers and import libraries.

# Copyright & License

In short: libsquashfs is LGPLv3 licensed, the utility programs are GPLv3.

Some 3rd party source code is included with more permissive licenses, some of
which is actually compiled into libsquashfs. Copyright notices for those must
be included when distributing either source or binaries of squashfs-tools-ng.

See [COPYING.md](COPYING.md) for more detailed information.

# Getting and Building the Source Code

Official release tarballs can be obtained here:

https://infraroot.at/pub/squashfs

The official git tree is available at the following locations:

https://github.com/AgentD/squashfs-tools-ng

https://git.infraroot.at/squashfs-tools-ng.git

Those locations are kept in sync and the former is a GitHub project that also
accepts and handles issues & pull requests.

If you are working on an official release tarball, you can build the package
like every autotools based package:

	./configure
	make
	make install

If you work on the git tree, you need to bootstrap the build system first:

	./autogen.sh

If Doxygen is available, a reference manual can be built as follows:

	make doxygen-doc

The pre-compiled binary packages for Windows are built using a helper script
that uses a MinGW cross toolchain to build squashfs-tools-ng and any of the
required dependencies:

	./mkwinbins.sh

## Structure of the Source Code

The main functionality of the package is split up into a number of libraries.
The actual tools are mainly wrappers around the libraries that combine their
functionality in a useful way.

The headers of all the libraries can be found in the `include` directory,
whereas the source code is in a per-library sub-directory within `lib`. The
tools themselves are in sub-directories within `bin`.

The `include` directory has a sub-directory `sqfs` which contains the public
headers of `libsquashfs.so` which are installed along with the library. All
other headers are private to this package.

The following components exist:
 - `libfstree.a` built from files in `lib/fstree` contains functions for
   manipulating a file system tree.
 - `libtar.a` built from files in `lib/tar` contains data structures and
   functions for parsing and creating tar files.
 - `libsquashfs.so` built from files in `lib/sqfs` contains all kinds of
   data structures for reading and writing SquashFS archives. Abstractions
   for data compression and so on. It contains the actual brains of this
   package.
 - `libcommon.a` built from files in `lib/common` contains a bunch
   of commonly used code shared across the utilities.
 - `libcompat.a` built from files in `lib/compat` contains minimal
   implementations of POSIX or GNU functions that are not available on some
   platforms.
 - `libutil.a` contains common utilities that are used internally by both the
   programs and `libsquashfs.so`.

Optionally, `libsquashfs` can be compiled with builtin, custom versions of zlib
and lz4. The configure options `--with-builtin-zlib` and `--with-builtin-lz4`
can be used. The respective library sources are also in the `lib` directory.

The `tests` sub-directory contains unit tests for the libraries.

The `extras` sub-directory contains a few demo programs that use `libsquashfs`.

To allow 3rd party applications to use `libsquashfs.so` without restricting
their choice of license, the code in the `lib/sqfs` and `lib/util` directories
is licensed under the LGPLv3, in contrast to the rest of this package.

## A Note on LZO Support

The SquashFS format supports compression using LZO. The `liblzo2` library
itself is released under the GNU GPL, version 2.

To make the `libsquashfs` library available as an LGPL library, it *cannot* be
linked against `liblzo2`, neither statically nor dynamically.

This legal problem has been solved using the following technical measure:

 - `libsquashfs`, as of right now, does not support LZO compression.
 - The `libcommon` helper library has an implementation of an `liblzo2` based
   compressor. This library and the tools that use it are released under
   the GPL.

This way, the tools themselves *do* support LZO compression seamlessly, while
the `libsquashfs` library does not.

## Automated Testing and Analysis

[![Build Status](https://travis-ci.com/AgentD/squashfs-tools-ng.svg?branch=master)](https://travis-ci.com/AgentD/squashfs-tools-ng)
[![Coverity Status](https://scan.coverity.com/projects/18718/badge.svg)](https://scan.coverity.com/projects/squashfs-tools-ng)

The GitHub project for squashfs-tools-ng is registered with Travis-CI and
Coverity Scan.

The [Travis-CI](https://travis-ci.com/github/AgentD/squashfs-tools-ng]) page
shows the current build status for various system configurations for the
latest commit on master, as well as pull requests on the GitHub project page.

The [Coverity Scan](https://scan.coverity.com/projects/squashfs-tools-ng) page
shows details for static analysis runs on the code, which are triggered
manually and thus run less frequently.

## Further Information

A documentation of the SquashFS on-disk format in plain text format can be
found in the [documentation directory](doc/format.txt), which is based on
an online version that can be found here:

https://dr-emann.github.io/squashfs/


The closest thing to an official web site can be found here:

https://infraroot.at/projects/squashfs-tools-ng/index.html

This location also hosts the Doxygen reference manual for the latest release.

There is currently no official mailing list. So far I used the squashfs-tools
mailing list on SourceForge for announcments and I will continue to do so
until I am booted off.
