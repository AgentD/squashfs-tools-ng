# License of squashfs-tools-ng

The `libsquashfs` library is released under the terms and conditions of the
**GNU Lesser General Public License version 3 or later**. This applies to
all source code in the directories `lib/sqfs`, `lib/util` and `include/sqfs`
with the following exceptions:

 - `lib/util/xxhash.c` contains a modified implementation of the xxhash32
   algorithm. See `licenses/xxhash.txt` for copyright and licensing
   information (2 clause BSD license).
 - `lib/lz4` contains files extracted from the LZ4 compression library.
   See `lib/lz4/README` for details and `licenses/LZ4.txt` for copyright and
   licensing information (2 clause BSD license).
 - `lib/zlib` contains files that have been extracted from the the zlib
   compression library and modified. See `lib/zlib/README` for details
   and `licenses/zlib.txt` for details.
 - `lib/util/hash_table.c`, `include/hash_table.h` and
   `lib/util/fast_urem_by_const.h` contain a hash table implementation (MIT
   license). See `licenses/hash_table.txt` for details.

The rest of squashfs-tools-ng is released under the terms and conditions of
the **GNU General Public License version 3 or later**, with the following
exceptions:

 - `lib/compat/fnmatch.c` has been copied from Musl libc.
 - `lib/compat/getopt.c` has been copied from Musl libc.
 - `lib/compat/getopt_long.c` has been copied from Musl libc.
 - `lib/compat/getsubopt.c` has been copied from Musl libc.

The components copied from Musl libc are subejct to an MIT style license.
See `liceneses/musl.txt` for details and only compiled into executable programs
if the target system does not provide an implementation.

Copies of the LGPLv3 and GPLv3 are included in `licenses/LGPLv3.txt` and
`licenses/GPLv3.txt` respectively.

The original source code of squashfs-tools-ng has been written by David
Oberhollenzer in 2019 and onward. Additional contributions have been added
since the initial release which makes some parts of the package subject to the
copyright of the respective authors. Appropriate copyright notices and SPDX
identifiers are included in the source code files.

Although the existing squashfs-tools and the Linux kernel implementation have
been used for testing, the source code in this package is neither based on,
nor derived from either of them.

## Documentation and the Build System

The auto-tools based build system has in large parts been hacked together by
copy & pasting from various tutorials and other projects (mostly util-linux,
and mtd-utils), overhauled many times since 2015.

The m4 macros in the `m4` directory were copied verbatim and have explicit
licenses. Please respect those. As for everything else, feel free to copy and
paste it as you wish.

The `doc` directory contains measurement data, pseudo lab reports and an RFC
style write-up of the SquashFS format. You may do with those as you please.

If you use those as a basis for writing about SquashFS or this package, please
cite your sources and mark verbatim quotations as such. I won't be angry if you
don't, but a thesis supervisor, reviewer or fellow Wikipedian might be.

# Binary Packages with 3rd Party Libraries

If this file is included in a binary release package, additional 3rd party
libraries may be included, which are subject to the copyright of their
respective authors and the terms and conditions of their respective licenses.

The following may be included:

 - The LZO compression library. Copyright Markus F.X.J. Oberhumer. This is
   released under the terms and conditions of the GNU General Public License
   version 2. A copy of the license is included in `licenses/GPLv2.txt`.
 - The LZ4 compression library. Copyright Yann Collet. This is released under a
   2 clause BSD style license, included in `licenses/LZ4.txt`. This library may
   be linked directly into `libsquashfs`, built from source code included in
   the source distribution.
 - The XZ utils liblzma library is released into the public domain. An excerpt
   from the `COPYING` file of its source code archive is included
   in `licenses/xz.txt`.
 - The zlib compression library. Copyright Jean-loup Gailly and Mark Adler.
   This is released under the terms and conditions of the zlib license,
   included in `licenses/zlib.txt`. This library may be linked directly
   into `libsquashfs`, built from source code included in the source
   distribution.
 - The zstd compression library. Copyright Facebook, Inc. All rights reserved.
   This is released under a BSD style license, included in `licenses/zstd.txt`.
 - Parts of the Musl C library. Copyright Rich Felker, et al.
   This is released under an MIT style license, included in `licenses/musl.txt`.

Independent of build configurations, the `libsquashfs` library contains
the following 3rd party source code, directly linked into the library:

 - A modified version of the xxhash32 hash function (Copyright Yann Collet).
   This is released under a 2-Clause BSD License. See `licenses/xxhash.txt`
   for details.
 - A hash table implementation liftet from the Mesa3D source code. This is
   released under the MIT/X11 license. See `licenses/hash_table.txt` for
   details.
