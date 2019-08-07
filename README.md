# About

SquashFS is a highly compressed, read only file system often used as a root fs
on embedded devices, live systems or simply as a compressed archive format.

Think of it as a .tar.gz that you can mount (or XZ, LZO, LZ4, ZSTD).

As the name suggests, this is not the original user space tooling for
SquashFS, which are currently maintained in parallel elsewhere. After a
long period of silence on the SourceForge site and mailing list, I
attempted to fork the existing code base with the intention to
restructure/clean it up and add many features I personally perceived to
be missing, but I ultimately decided that it would be easier to start
from scratch than to work with the existing code.

Here are some of the features that primarily distinguish this package from
the (at the time of writing recent) squashfs-tools 4.3:

 - Reproducible SquashFS images, i.e. deterministic packing without
   any local time stamps.
 - Linux `gen_init_cpio` like file listing for micro managing the
   file system contents, permissions, and ownership without having to replicate
   the file system (and especially permissions) locally.
 - Support for SELinux contexts file (see selabel_file(5)) to generate
   SELinux labels.
 - Structured and (hopefully) more readable source code that should be better
   maintainable in the long run.


In addition to that, tools have been added to directly convert a tar archive
into a SquashFS filesystem image and back. This allows for using existing
tools can work on tar archives seamlessly on SquashFS images.


The tools in this package have different names, so they can be installed
together with the existing tools:

 - `gensquashfs` can be used to produce SquashFS images from `gen_init_cpio`
   like file listings or simply pack an input directory.
 - `rdsquashfs` can be used to inspect and unpack SquashFS images.
 - `sqfs2tar` can turn a SquashFS image into a tarball, written to stdout.
 - `tar2sqfs` can turn a tarball (read from stdin) into a SquashFS image.
 - `sqfsdiff` can compare the contents of two SquashFS images.

# Getting and Building the Source Code

Official release tarballs can be obtained here:

https://infraroot.at/pub/squashfs

The official git tree is currently located here:

https://github.com/AgentD/squashfs-tools-ng

If you are working on an official release tarball, you can build the package
like every autotools based package:

	./configure
	make
	make install

If you work on the git tree, you need to bootstrap the build system first:

	./autogen.sh

## Structure of the Source Code

The main functionality of the package is split up into a number of static
libraries. The actual tools are mainly wrappers around the libraries that
combine their functionality in a useful way.

The headers of all the libraries can be found in the `include` directory,
whereas the source code is in a per-library sub-directory within `lib`.

The following components exist:
 - `libutil.a` built from files in `lib/util` contains miscellaneous helper
   functions.
 - `libcompress.a` built from files in `lib/comp/` contains an abstract
   interface for block compression and concrete implementations using
   different compressor libraries. It uses an enum from `squashfs.h` to
   identify compressors but it only depends on functions from `libutil.a`.
   A program using `libcompress.a` also has to link against the various
   compressor libraries.
 - `libfstree.a` built from files in `lib/fstree` contains functions for
   manipulating a file system tree. It only depends on `libutil.a`
 - `libtar.a` built from files in `lib/tar` contains data structures and
   functions for parsing and creating tar files. It only depends
   on `libutil.a`.
 - `libsquashfs.a` built from files in `lib/sqfs` contains all kinds of
   data structures for reading and writing SquashFS archives. It is built
   on top of `libutil.a`, `libfstree.a` and `libcompress.a`.

The headers in `include` are stuffed with comments on functions an data
structures.

The `tests` sub-directory contains unit tests for the libraries.

## Further Information

A documentation of the SquashFS on-disk format can be found here:

https://dr-emann.github.io/squashfs/

# Copyright & License

The software in this package is released under the terms and conditions of the
GNU General Public License version 3 or later. The file `LICENSE` contains a
copy of the license.

The original source code in this package has been written by David
Oberhollenzer in 2019. Additional contributions have been added since the
initial release which makes some parts of the package subject to the copyright
of the respective authors.

Although the existing squashfs-tools and the Linux kernel implementation have
been used for testing, the source code in this package is neither based on,
nor derived from either of them.
