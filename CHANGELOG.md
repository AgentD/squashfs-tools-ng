# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.1.2] - 2021-06-25
### Added
- Test cases for concatenated stream decompression
- A more "real-world" test suite for `tar2sqfs` pre-release testing

### Fixed
- Replace tabs with spaces in format.txt
- Some documentation clarifications and typo fixes
- libsquashfs: preserve alignment flag in block processor
- libsquashfs: broken block alignment in block write
- allow concatenated Bzip2 streams
- Use Automake conditional for zstd stream compression support
- Use *_MAX from limits.h instead of configure-time type size checks
- Additional compiler warnings were turned on and addressed
- libfstream: Add printf format specifier attribute
- libfstream: guard against potential integer overflows
- libfstree: guard against link count and inode number overflow
- libfstree: guard against possible overflow in readlink()
- libcommon: potentially un-aligned data access in LZO compressor
- libsquashfs: potentially unaligned data access in meta data handling
- Some format string type/signedness mismatch issues

## [1.1.1] - 2021-05-07
### Fixed
- tar2sqfs: currectly process concatenated xz streams from parallel compression
- sqfs2tar: don't report an error if fsync() returns EINVAL
- libsquashfs: static linking on Windows
- libsquashfs: visibillity of internal mempool functions
- libsquashfs: add sqfs_free() function, mainly for Windows portabillity
- libsquashfs: block processor: Fix account for manually submitted blocks
- Fix build failure if configured with --without-tools

## [1.1.0] - 2021-03-28
### Added
- tar2sqfs: support transparently reading stream compressed archives
- sqfs2tar: support creating stream compressed archives
- gensquashfs: support file globbing/filtering in the description file
- Support bzip2 compression for tar
- Implement directory scanning for Windows
- a `glob` keyword to `gensquashfs` for `find` like globbing from a directory.

### Changed
- Rewrite file I/O in the tools around an I/O stream wrapper which is used
  to implement the transparent compression.
- Internal cleanups & restructuring, trying to improve maintainabillity and
  testabillity of the code.
- Updated benchmarks, including benchmarks for decompression.
- Drastic performance improvements of the xattr writer (#68).
- Performance improvements in the thread pool block processor.
- Bump zstd & xz versions for Windows binary releases.

### Fixed
- libsquashfs: Allow shared read access to generated images on Windows (#79)

## [1.0.4] - 2021-01-23
### Fixed
- typos in documentation
- libsquashfs: xattr_writer: fail if allocation fails
- remove broken normalization of backslashes (#77, #78)
- congestion in block processor when processing lots of tiny fragments
- exact byte-for-byte matching during block deduplication
- exact byte-for-byte matching during fragment deduplication

### Added
- new, extensible block processor create function required for
  deduplication fixes

## [1.0.3] - 2020-11-05
### Fixed
- tar2sqfs: if --root-becomes is used, also retarget links (#70)
- tar2sqfs: when skipping non-prefixed files, also skip contents (#70)
- tar2sqfs: null-pointer dereference if a file with a broken path is skipped
- rdsquashfs: correctly escape of the input file paths if prefixed

### Added
- Packing scripts for a various GNU/Linux distributions (#65, #67)

## [1.0.2] - 2020-09-03
### Fixed
- Fix block processor single block with don't fragment flag bug (#63)
- Fix libtar treatment of link targets that fill the header field (#64)
- Fix tree node path generation for detached sub trees (#66)
- Fix rdsquashfs unpack under Windows if a directory exists
- Clarify copyright status of documentation & build system

### Added
- Support for GNU tar sparse format 1.0.
- A spec file to build RPM packages (#65).
- A `--stat` option to rdsquashfs

## [1.0.1] - 2020-08-03
### Fixed
- Xattr reader: re-read the header after seaking to an OOL value (#60)
- Documentation: mention the file name limit imposed by the kernel
- Documentation: fix wrong magic value and stray tabs in format.txt (#61)
- Fix block bounds checking in libsquashfs data reader (#58)
- Fix build issue caused by demo code that didn't have (#57, #59)

## [1.0.0] - 2020-06-13
### Added
- Expose more fine grained control values & flags on the XZ and LZMA compressors
- A flag for the `libsquashfs` block processor to micro manage block hashing and
  sparse block detection.
- A raw block submission function.
- A user pointer that can be forwarded to the block writer.

### Changed
- `sqfsdiff` doesn't abort if it fails to read the compressor options
- in `libsquashfs`, turn the block writer into an interface with a default
  implementation and remove the statistics and hooks.
- Block processor can function without fragment table and without inode pointer
- Strictly enfore min/max dictionary size in XZ & LZMA compressors
- Make compression level a generic compression option in `libsquashfs`
- More `libsquashfs` API and internal cleanups
- Bump the major version number of libsquashfs

### Fixed
- Propperly set the last block flag if fragments are disabled
- Compilation on GCC4 and below
- libtar: size computation of PAX line length (#50)
- Semantics of the super block deduplication
- Actually set the ZSTD compression level to something greater than 0
- Only add Selinux compile flags if WITH_SELINUX is set. Fixes Mingw cross build
  on Fedora.
- Make `rdsquashfs` describe mode terminate with an error message if an illegal
  filename is encountered (#52).
- Don't include alloca.h on systems that don't provide that header.

## [0.9.1] - 2020-05-03
### Added
- Options to `gensquashfs` for overriding the ownership if packing a directory.

### Changed
- Various internal code and build system related cleanups.
- Various performance improvements for `gensquashfs` and `tar2sqfs`.
- Make `tar2sqfs` compeltely ignore PAX global headers,
  even if `--no-skip` is set (#45).
- Make `gensquashfs` fail if extra arguments are passed, similar to `tar2sqfs`.

### Fixed
- Missing assert header inclusion if built with `--without-lzo` (#41)
- sqfs2tar: also emit trailing slashes for empty directories (#42)
- Illegal implicit cast in public `libsquashfs` headers if used from C++.
- If tar2sqfs or gensquashfs fails, delete the output file.
- Make sure test cases also work if built with NDEBUG defined.
- Add missing `--with-gzip` configure option.
- In `tar2sqfs`, actually apply the `--no-tail-packing` if set.
- Potential nondeterministic order of extended attributes
  in `gensquashfs` if packing a directory.
- Manpages lagging behind.

## [0.9.0] - 2020-03-30
### Added
- Support parsing [device] block size argument with SI suffix.
- Add a write-up on the on-disk format.
- A couple demo programs that make use of `libsquashfs`.
- Add statistics counters to the block writer and processor.
- A compressor interface function to retrieve its configuration.

### Changed
- For better compatibility, sqfs2tar appends `/` to directory names. (#37)
- Extra data in sqfs_inode_generic_t no longer handled via payload pointers.
- Add a currently unsued flag field to the id table create function.
- Split off fragment table handling from data reader and writer into
  dedicated fragment table data type.
- Split data writer up into a block writer and a block processor.
- All abstract data types inhert from an sqfs_object_t with common
  functionality.
- Lots of performance improvements.
- Make block processor sync and flush seperate operations.
- Combined the various payload size values in generic inode structure.
- Change block processor API to manage creation/resizing of file inodes.
- A "do not deduplicate flag" for the block processing pipeline.
- Lots of API cleanups.

### Fixed
- Include sys/sysmacros.h on any GNU libc platform.
- Directory index accounting.
- Memory leak in hard link detection code.
- Broken iteration over directory children in sqfsdiff.
- Data reader returning -1 instead of an error code.
- Size accounting for sparse files in tar parsing code.
- Stricter verification of the compressor configuration.
- Broken builds with older liblz4 and zstd versions (e.g. on Ubuntu Xenial).
- Various build issues on MacOS.

### Removed
- A number of hook callbacks from the block writer.
- sqfs_block_t from the public API.

## [0.8.0] - 2019-12-30
### Added
- Experimental Windows port using MinGW cross compilation toolchain.
- Port to BSD systems.
- Explicit argument invalid error code in `libsquashfs`.
- A `--root-becomes` option to `tar2sqfs` and `sqfs2tar`.
- A `--no-tail-packing` option to `tar2sqfs` and `sqfs2tar`.
- CHANGELOG.md now references GitHub issue numbers.
- Simple integration and regression test suit.
- Simple creation of an NFS export table throught `libsquashfs`.
- Support for non-directory hard links in `gensquashfs`, `tar2sqfs`
  and `sqfs2tar`.

### Changed
- Return propper error code from `sqfs_get_xattr_prefix_id`.
- Return propper error code from `sqfs_compressor_id_from_name`.
- Combined error and value return from `sqfs_compressor_id_from_name`.
- Return error code from compressor functions if input is larger than 2G.
- Lots of cleanups, including the build system.

### Removed
- Entirely redundant `sqfs_has_xattr` function from `libsquashfs`.

### Fixed
- LZO compressor moved out of libsquashfs to avoid licensing problems.
- Make overriding configure variables for LZO library actually work.
- Do not follow symlinks when reading xattrs from input files.
- Ignore directory entry named `./` in tar2sqfs. (#22, #31)
- Reject empty string as directory name in libsquashfs. (#22, #31)
- Fix memory leak in tar2sqfs if entries are skipped.
- Fix tar_fuzz error check after seek.
- Fix the `fstree_init` test to account for defaults from `SOURCE_DATE_EPOCH`.
- Honor the no_xattr flag when generating SquashFS images.
- Block size check in `sqfs_super_init`. (#29)
- Fix pthread block processor interfering with application signal handling.
- Added pthread flags to the programs using libsquashfs.
- Fix "buffer too small" being treated as fatal error by the zstd compressor.
- Fix out of bounds write in the LZO compressor.
- Fix queue accounting in the compressor thread pool. (#29)
- Fix name of `libsquashfs` pkg-config file.
- Reading of binary (i.e. non-textual) xattr values from tar files. (#32)
- A bug in parsing the GNU.sparse.name PAX attribute from tar files.
- sqfsdiff: recurse into directories that are only in one image.

## [0.7.0] - 2019-10-08
### Added
- LGPLv3 licensed, shared library `libsquashfs.so` containing all the SquashFS
  related logic.
- Sanitized, public headers and pkg-config file for libsquashfs.
- Doxygen reference manual for libsquashfs.
- Legacy LZMA compression support. (#17)
- User configurable queue backlog for tar2sqfs and gensquashfs.

### Changed
- Make sqfsdiff continue comparing even if the types are different,
  but compatible (e.g. extended directory vs basic directory).
- Try to determine the number of available CPU cores and use the
  maximum by default.
- Start numbering inodes at 1, instead of 2.
- Only store permission bits in inodes, the reader reconstructs them from the
  inode type.
- Make "--keep-time" the default for tar2sqfs and use flag to disable it.

### Fixed
- An off-by-one error in the directory packing code. (#18)
- Typo in configure fallback path searching for LZO library.
- Typo that caused LZMA2 VLI filters to not be used at all.
- Possible out-of-bounds access in LZO compressor constructor.
- Inverted logic in sqfs2tar extended attributes processing.

### Removed
- Comparisong with directory from sqfsdiff.

## [0.6.1] - 2019-08-27
### Added
- Add a change log
- Add test programs for fuzzing tar and gensquashfs file format parsers

### Fixed
- Harden against integer overflows when parsing squashfs images (#13, #14)
- Test against format limits when parsing directory entries (#12)
- More thorough bounds checking when reading metadata blocks (#13, #14, #15)

## [0.6.0] - 2019-08-22
### Added
- New utility `sqfsdiff` that can compare squashfs images
- rdsquashfs can now dump extended attributes for an inode (#2)
- rdsquashfs can now optionally set xattrs on unpacked files (#2)
- rdsquashfs can now optionally restore timestamps on unpacked files (#2)
- sqfs2tar can now optionally copy xattrs over to the resulting tarball (#2)
- gensquashfs can now optionally read xattrs from input files (#2)
- gensquashfs now has a --one-file-system option
- tar2sqfs and gensquashfs now output some simple statistics
- Full fragment and data block deduplication
- Support for SOURCE_DATE_EPOCH environment variable
- Optimized, faster file unpacking order
- Faster, pthread based, parallel block compressor

### Fixed
- Return the correct value from data_reader_create
- Fix free() of stack pointer in id_table_read error path
- Fix missing initialization of file fragment fields
- Fix xattr OOL position
- Fix super block flags: clear "no xattr" flag when writing xattrs
- Fix xattr writer size accounting
- Fix explicit NULL dereference in deserialize_fstree failure path
- Fix tar header error reporting on 32 bit systems
- Make sure file listing generated by rdsquashfs -d is properly escaped
- Fix functions with side effect being used inside asserts
- Fix zero padding of extracted data blocks
- Fix forward seek when unpacking sparse files
- Fix wrong argument type for gensquashfs --keep-time
- Fix memory leak in dir-scan error code path
- Fix chmod of symlinks in restore_fstree
- Add proper copyright headers to all source files

### Changed
- Various internal data structure and code cleanups
- Overhaul README and convert it to markdown

## [0.5.0] - 2019-07-28
### Added
- Support for NFS export
- Support for xattr value deduplication
- Flag in packers to optionally keep the original time stamps
- Largefile support
- Implement simple, fork() based parallel unpacking in rdsquashfs

### Fixed
- Remove unfriendly words about squashfs-tools from README (#10)
- Propper error message for ZSTD compressor
- Correct copy-and-paste mistake in the build system
- Make sure xattr string table is propperly initialized
- More lenient tar checksum verification
- Fix xattr unit test
- Fix possible leak in tar2sqfs if writing xattrs fails
- Fix corner cases in directory list parsing
- Fix processing of tar mtime on 32 bit systems (#8)
- libfstree: fix signed/unsigned comparisons
- Fix fragment reader out of bounds read when loading table
- Fix checks of super block block size
- Fix potential resource leak in deserialize_tree
- Enforce reasonable upper and low bounds on the size of tar headers
- Make sure target in fstree_mknode is always set when creating a symlink
- Use safer string copy function to fill tar header

## [0.4.2] - 2019-07-17
### Fixed
- Sanity check id table size read from super block
- Various bug fixes through Coverity scan
- Fix dirindex writing for ext dir inode
- fstree: mknode: initialize fragment data, add extra blocksize slot
- Fix directory index creation
- Support for reading files without fragments
- Support for spaces in filenames
- Eleminate use of temporary file

## [0.4.1] - 2019-07-07
### Fixed
- read_inode: determine mode bits from inode type
- Actually encode/decode directory inode difference as signed
- Fix regression in fstree_from_file device node format
- Always initialize gensquashfs defaults option

## [0.4.0] - 2019-07-04
### Added
- no-skip option in tar2sqfs and sqfs2tar
- Option for sqfs2tar to extract only some subdirectories
- Support for xattr extensions in tar parser
- Support repacking xattrs from tarball into squashfs filesystem

### Fixed
- Null-pointer dereference in restore_unpack
- Memory leak in gzip compressor
- Stack pointer free() in fstree_from_dir
- Use of uninitialized xattr structure
- Initialize return status in fstree_relabel_selinux
- Make pax header parser bail if parsing a number fails
- Double free in GNU tar sparse file parser
- Never used overflow error message in fstree_from_file
- Unused variable assignment in tar header writer
- Make sure fragment and id tables are initialized
- Directory index offset calculation
- Missing htole32 transformations
- Don't blindly strcpy() the tar header name
- Typos in README
- Composition order of prefix + name for ustar
- Actually check return value when writing xattrs
- Possible out of bounds read in libcompress.a
- Check block_log range before deriving block size from it
- tar2sqfs: check for invalid file names first

### Changed
- Tar writer: Use more widely supported GNU tar extensions instead of PAX
- Simplify deduction logic for squashfs inode type

## [0.3.0] - 2019-06-30
### Added
- Add utility to turn a squashfs image into a POSIX tar archvie
- Add utility to turn a POSIX/PAX tar archive into a squashfs image
- Add unit tests
- Add support for packing sparse files
- Add support for unpacking sparse files as sparse files

### Fixed
- Actually update permissions in fstree add by path
- Always set permissions on symlinks to 0777
- gensquashfs: Fix typo in help text
- Fix inode fragment & sparse counter initialization
- Ommit fragment table if there really are no fragments

### Changed
- Lots of internal cleanups and restructuring

## [0.2.0] - 2019-06-13
### Fixed
- Make empty directories with xattrs work
- Flush the last, unfinished fragment block

### Changed
- Add pushd/popd utility functions and replace directory traversal code
- Lots of internal cleanups
- Use abstractions for many operations and move them to support libraries

## [0.1.0] - 2019-06-08
### Added
- Salvage protoype from Pygos project and turn it into generic squashfs packer
- Add unpacker

### Changed
- Insert abstraction layers and split generic code off into support libraries

[1.1.2]: https://github.com/AgentD/squashfs-tools-ng/compare/v1.1.1...v1.1.2
[1.1.1]: https://github.com/AgentD/squashfs-tools-ng/compare/v1.1.0...v1.1.1
[1.1.0]: https://github.com/AgentD/squashfs-tools-ng/compare/v1.0.4...v1.1.0
[1.0.4]: https://github.com/AgentD/squashfs-tools-ng/compare/v1.0.3...v1.0.4
[1.0.3]: https://github.com/AgentD/squashfs-tools-ng/compare/v1.0.2...v1.0.3
[1.0.2]: https://github.com/AgentD/squashfs-tools-ng/compare/v1.0.1...v1.0.2
[1.0.1]: https://github.com/AgentD/squashfs-tools-ng/compare/v1.0.0...v1.0.1
[1.0.0]: https://github.com/AgentD/squashfs-tools-ng/compare/v0.9.1...v1.0.0
[0.9.1]: https://github.com/AgentD/squashfs-tools-ng/compare/v0.9...v0.9.1
[0.9.0]: https://github.com/AgentD/squashfs-tools-ng/compare/v0.8...v0.9
[0.8.0]: https://github.com/AgentD/squashfs-tools-ng/compare/v0.7...v0.8
[0.7.0]: https://github.com/AgentD/squashfs-tools-ng/compare/v0.6.1...v0.7
[0.6.1]: https://github.com/AgentD/squashfs-tools-ng/compare/v0.6...v0.6.1
[0.6.0]: https://github.com/AgentD/squashfs-tools-ng/compare/v0.5...v0.6
[0.5.0]: https://github.com/AgentD/squashfs-tools-ng/compare/v0.4...v0.5
[0.4.2]: https://github.com/AgentD/squashfs-tools-ng/compare/v0.4.1...v0.4.2
[0.4.1]: https://github.com/AgentD/squashfs-tools-ng/compare/v0.4...v0.4.1
[0.4.0]: https://github.com/AgentD/squashfs-tools-ng/compare/v0.3...v0.4
[0.3.0]: https://github.com/AgentD/squashfs-tools-ng/compare/v0.2...v0.3
[0.2.0]: https://github.com/AgentD/squashfs-tools-ng/compare/v0.1...v0.2
[0.1.0]: https://github.com/AgentD/squashfs-tools-ng/releases/tag/v0.1
