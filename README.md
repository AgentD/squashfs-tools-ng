# About

SquashFS is a highly compressed, read only file system often used as a root fs
on embedded devices, live systems or simply as a compressed archive format.

Think of it as a .tar.gz that you can mount (or XZ, LZO, LZ4, ZSTD).

As the name suggests, this is not the original user space tooling for
SquashFS, which is currently maintained in parallel elsewhere. After a
long period of silence on the SourceForge site and mailing list, I
attempted to fork the existing code base with the intention to
restructure/clean it up and add many features I personally perceived to
be missing, but I ultimately decided that it would be easier to start
from scratch than to work with the existing code.

Here are some of the features that primarily distinguish this package from
the squashfs-tools 4.3 (latest recent version at the time this project was
started):

 - A shared library that encapsulates code for accessing SquashFS images,
   usable by 3rd party applications.
 - Reproducible, deterministic SquashFS images.
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


Most of the actual logic of those tools is implemented in the `libsquashfs.so`
library that (by default) gets installed on the system along with its header
files, allowing 3rd party applications to use it (e.g. for embedding SqushFS
inside a custom container format without having to implement the SquashFS
part).

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

The main functionality of the package is split up into a number libraries.
The actual tools are mainly wrappers around the libraries that combine their
functionality in a useful way.

The headers of all the libraries can be found in the `include` directory,
whereas the source code is in a per-library sub-directory within `lib`.

The `include` directory has a sub-directory `sqfs` which contains the public
headers of `libsquashfs.so` which are installed along with the library. All
other headers are private to this package.

The following components exist:
 - `libutil.la` built from files in `lib/util` contains miscellaneous helper
   functions.
 - `libfstree.a` built from files in `lib/fstree` contains functions for
   manipulating a file system tree. It only depends on `libutil.la`
 - `libtar.a` built from files in `lib/tar` contains data structures and
   functions for parsing and creating tar files. It only depends
   on `libutil.la`.
 - `libsquashfs.so` built from files in `lib/sqfs` contains all kinds of
   data structures for reading and writing SquashFS archives. Abstractions
   for data compression and so on. It contains the actual brains of this
   package. It uses `libutil.la` internally.
 - `libsqfshelper.a` built from files in `lib/sqfshelper` contains a bunch
   of high level glue code that combines `libfstree.a` and `libsquashfs.so`
   and provides common functionality used by all the tools.

The headers in `include` are stuffed with comments on functions an data
structures.

The `tests` sub-directory contains unit tests for the libraries.

To allow 3rd party applications to use `libsquashfs.so` without restricting
their choice of license, the code in the `lib/sqfs` and `lib/util`
sub-directories is licensed under the LGPLv3, in contrast to the rest of this
package.

## Further Information

A documentation of the SquashFS on-disk format can be found here:

https://dr-emann.github.io/squashfs/

# Copyright & License

The code of the `libsquashfs` library is released under the terms and
conditions of the GNU Lesser General Public License version 3 or later. The
file `LICENSE-lgpl.txt` contains a copy of the license. Since the license is
based on the General Public License version 3 and makes references to it
instead of duplicating text, a copy of the GPL version 3 is also provided
by the file `LICENSE-gpl.txt`.

The rest of the software in this package is released under the terms and
conditions of the GNU General Public License version 3 or later. The
file `LICENSE-gpl.txt` contains a copy of the license.

The original source code in this package has been written by David
Oberhollenzer in 2019. Additional contributions have been added since the
initial release which makes some parts of the package subject to the copyright
of the respective authors.

Although the existing squashfs-tools and the Linux kernel implementation have
been used for testing, the source code in this package is neither based on,
nor derived from either of them.
