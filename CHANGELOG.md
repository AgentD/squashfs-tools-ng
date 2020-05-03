# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

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

[Unreleased]: https://github.com/AgentD/squashfs-tools-ng/compare/v0.9...HEAD
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
