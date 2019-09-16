/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * dir.h - This file is part of libsquashfs
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#ifndef SQFS_DIR_H
#define SQFS_DIR_H

#include "sqfs/predef.h"

/**
 * @file dir.h
 *
 * @brief Contains on-disk data structures for the directory table and
 *        declarations for the @ref sqfs_dir_writer_t.
 */

/**
 * @struct sqfs_dir_writer_t
 *
 * @brief Abstracts generating of directory entries
 *
 * SquashFS stores directory entries and inodes seperated from each other. The
 * inodes are stored in a series of meta data blocks before another series of
 * meta data blocks that contain the directory entries. Directory inodes point
 * to meta data block (and offset) where its contents are listed and the
 * entries in turn point back to the inodes that represent them.
 *
 * There are some rules to this. Directory entries have to be written in
 * ASCIIbetical ordering. Up to 256 entries are preceeded by a header. The
 * entries use delta encoding for inode numbers and block locations relative to
 * the header, so every time the inodes cross a meta data block boundary, if
 * the difference in inode number gets too large, or if the entry count would
 * exceed 256, a new header has to be emitted. Even if the inode pointed to is
 * an extended type, the entry in the header still has to indicate the base
 * type.
 *
 * In addtion to that, extended directory inodes can contain an index for
 * faster lookup. The index points to each header and requires a new header to
 * be emitted if the entries cross a block boundary.
 *
 * The dir writer takes care of all of this and provides a simple interface for
 * adding entries. Internally it fills data into a meta data writer and
 * generates an index that it can, on request, write to another meta data
 * writer used for inodes.
 */

/**
 * @struct sqfs_dir_reader_t
 *
 * @brief Abstracts reading of directory entries
 *
 * SquashFS stores directory listings and inode structures seperated from
 * each other in meta data blocks.
 *
 * The sqfs_dir_reader_t abstracts access to the filesystem tree in a SquashFS
 * through a fairly simple interface. It keeps two meta data readers internally
 * for reading directory listings and inodes. Externally, it offers a few
 * simple functions for iterating over the contents of a directory that
 * completely take care of fetching/decoding headers and sifting through the
 * multi level hierarchie used for storing them on disk.
 *
 * See @ref sqfs_dir_writer_t for an overview on how directory entries are
 * stored in SquashFS.
 *
 * The reader also abstracts easy access to the underlying inodes, allowing
 * direct access to the inode referred to by a directory entry.
 */


#define SQFS_MAX_DIR_ENT 256

/**
 * @struct sqfs_dir_header_t
 *
 * @brief On-disk data structure of a directory header
 *
 * See @ref sqfs_dir_writer_t for an overview on how SquashFS stores
 * directories on disk.
 */
struct sqfs_dir_header_t {
	/**
	 * @brief The number of @ref sqfs_dir_entry_t entries that are
	 *        following.
	 *
	 * This value is stored off by one and the total count must not
	 * exceed 256.
	 */
	uint32_t count;

	/**
	 * @brief The location of the meta data block containing the inodes for
	 *        the entries that follow, relative to the start of the inode
	 *        table.
	 */
	uint32_t start_block;

	/**
	 * @brief The inode number of the first entry.
	 */
	uint32_t inode_number;
};

/**
 * @struct sqfs_dir_entry_t
 *
 * @brief On-disk data structure of a directory entry. Many of these
 *        follow a single @ref sqfs_dir_header_t.
 *
 * See @ref sqfs_dir_writer_t for an overview on how SquashFS stores
 * directories on disk.
 */
struct sqfs_dir_entry_t {
	/**
	 * @brief An offset into the uncompressed meta data block containing
	 *        the coresponding inode.
	 */
	uint16_t offset;

	/**
	 * @brief Signed difference of the inode number from the one
	 *        in the @ref sqfs_dir_header_t.
	 */
	int16_t inode_diff;

	/**
	 * @brief The @ref E_SQFS_INODE_TYPE value for the inode that this
	 *        entry represents.
	 */
	uint16_t type;

	/**
	 * @brief The size of the entry name
	 *
	 * This value is stored off-by-one.
	 */
	uint16_t size;

	/**
	 * @brief The name of the directory entry (no trailing null-byte).
	 */
	uint8_t name[];
};

/**
 * @struct sqfs_dir_index_t
 *
 * @brief On-disk data structure of a directory index. A series of those
 *        can follow an @ref sqfs_inode_dir_ext_t.
 *
 * See @ref sqfs_dir_writer_t for an overview on how SquashFS stores
 * directories on disk.
 */
struct sqfs_dir_index_t {
	/**
	 * @brief Linear byte offset into the decompressed directory listing.
	 */
	uint32_t index;

	/**
	 * @brief Location of the meta data block, relative to the directory
	 *        table start.
	 */
	uint32_t start_block;

	/**
	 * @brief Size of the name of the first entry after the header.
	 *
	 * This value is stored off-by-one.
	 */
	uint32_t size;

	/**
	 * @brief Name of the name of the first entry after the header.
	 *
	 * No trailing null-byte.
	 */
	uint8_t name[];
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a directory writer.
 *
 * @memberof sqfs_dir_writer_t
 *
 * @param dm A pointer to a meta data writer that the generated directory
 *           entries should be written to.
 *
 * @return A pointer to a directory writer on success, NULL on
 *         allocation failure.
 */
SQFS_API sqfs_dir_writer_t *sqfs_dir_writer_create(sqfs_meta_writer_t *dm);

/**
 * @brief Destroy a directory writer and free all its memory.
 *
 * @memberof sqfs_dir_writer_t
 *
 * @param writer A pointer to a directory writer object.
 */
SQFS_API void sqfs_dir_writer_destroy(sqfs_dir_writer_t *writer);

/**
 * @brief Begin writing a directory, i.e. reset and initialize all internal
 *        state neccessary.
 *
 * @memberof sqfs_dir_writer_t
 *
 * @param writer A pointer to a directory writer object.
 *
 * @return Zero on success, a @ref E_SQFS_ERROR value on failure.
 */
SQFS_API int sqfs_dir_writer_begin(sqfs_dir_writer_t *writer);

/**
 * @brief Add add a directory entry.
 *
 * @memberof sqfs_dir_writer_t
 *
 * @param writer A pointer to a directory writer object.
 * @param name The name of the directory entry.
 * @param inode_num The inode number of the entry.
 * @param inode_ref A reference to the inode, i.e. the meta data block offset
 *                  is stored in bits 16 to 48 and the lower 16 bit hold an
 *                  offset into the block.
 * @param mode A file mode, i.e. type and permission bits from which the entry
 *             type is derived internally.
 *
 * @return Zero on success, a @ref E_SQFS_ERROR value on failure.
 */
SQFS_API int sqfs_dir_writer_add_entry(sqfs_dir_writer_t *writer,
				       const char *name,
				       uint32_t inode_num, uint64_t inode_ref,
				       mode_t mode);

/**
 * @brief Finish writing a directory listing and write everything out to the
 *        meta data writer.
 *
 * @memberof sqfs_dir_writer_t
 *
 * @param writer A pointer to a directory writer object.
 *
 * @return Zero on success, a @ref E_SQFS_ERROR value on failure.
 */
SQFS_API int sqfs_dir_writer_end(sqfs_dir_writer_t *writer);

/**
 * @brief Get the total, uncompressed size of the last written
 *        directory in bytes.
 *
 * @memberof sqfs_dir_writer_t
 *
 * Call this function after @ref sqfs_dir_writer_end to get the uncompressed
 * size of the directory listing that is required for the directory inodes.
 * And also to determine which kind of directory inode to create.
 *
 * @param writer A pointer to a directory writer object.
 *
 * @return The size of the entire, uncompressed listing in bytes.
 */
SQFS_API size_t sqfs_dir_writer_get_size(const sqfs_dir_writer_t *writer);

/**
 * @brief Get the numer of entries written to the last directory.
 *
 * @memberof sqfs_dir_writer_t
 *
 * Call this function after @ref sqfs_dir_writer_end to get the total
 * number of entries written to the directory.
 *
 * @param writer A pointer to a directory writer object.
 *
 * @return The number of entries in the directory.
 */
SQFS_API
size_t sqfs_dir_writer_get_entry_count(const sqfs_dir_writer_t *writer);

/**
 * @brief Get the location of the last written directory.
 *
 * @memberof sqfs_dir_writer_t
 *
 * Call this function after @ref sqfs_dir_writer_end to get the location of
 * the directory listing that is required for the directory inodes.
 *
 * @param writer A pointer to a directory writer object.
 *
 * @return A meta data reference, i.e. bits 16 to 48 contain the block start
 *         and the lower 16 bit an offset into the uncompressed block.
 */
SQFS_API uint64_t
sqfs_dir_writer_get_dir_reference(const sqfs_dir_writer_t *writer);

/**
 * @brief Get the size of the index of the last written directory.
 *
 * @memberof sqfs_dir_writer_t
 *
 * Call this function after @ref sqfs_dir_writer_end to get the size of
 * the directory index that is required for extended directory inodes.
 *
 * @param writer A pointer to a directory writer object.
 *
 * @return The number of index entries.
 */
SQFS_API size_t sqfs_dir_writer_get_index_size(const sqfs_dir_writer_t *writer);

/**
 * @brief Write the index of the index of the last written directory to
 *        a meta data writer after the extended directory inode.
 *
 * @memberof sqfs_dir_writer_t
 *
 * @param writer A pointer to a directory writer object.
 * @param im A pointer to a meta data writer to write the index to.
 *
 * @return Zero on success, a @ref E_SQFS_ERROR value on failure.
 */
SQFS_API int sqfs_dir_writer_write_index(const sqfs_dir_writer_t *writer,
					 sqfs_meta_writer_t *im);

/**
 * @brief Helper function for creating an inode from the last directory.
 *
 * @memberof sqfs_dir_writer_t
 *
 * Call this function after @ref sqfs_dir_writer_end to create a bare bones
 * inode structure for the directory. The directory information is filled in
 * completely and the type is set, the rest of the basic information such as
 * permission bits, owner and timestamp is left untouched.
 *
 * If the generated inode is an extended directory inode, you can use another
 * convenience function called @ref sqfs_dir_writer_write_index to write the
 * index meta data after writing the inode itself.
 *
 * @param writer A pointer to a directory writer object.
 * @param hlinks The number of hard links pointing to the directory.
 * @param xattr If set to something other than 0xFFFFFFFF, an extended
 *              directory inode is created with xattr index set.
 * @param parent_ino The inode number of the parent directory.
 *
 * @return A generic inode or NULL on allocation failure.
 */
SQFS_API sqfs_inode_generic_t
*sqfs_dir_writer_create_inode(const sqfs_dir_writer_t *writer, size_t hlinks,
			      uint32_t xattr, uint32_t parent_ino);

/**
 * @brief Create a directory reader.
 *
 * @memberof sqfs_dir_reader_t
 *
 * @param super A pointer to the super block. Kept internally an used for
 *              resolving table positions.
 * @param cmp A compressor to use for unpacking meta data blocks.
 * @param file The input file to read from.
 *
 * @return A new directory reader on success, NULL on allocation failure.
 */
SQFS_API sqfs_dir_reader_t *sqfs_dir_reader_create(const sqfs_super_t *super,
						   sqfs_compressor_t *cmp,
						   sqfs_file_t *file);

/**
 * @brief Cleanup a directory reader and free all its memory.
 *
 * @memberof sqfs_dir_reader_t
 */
SQFS_API void sqfs_dir_reader_destroy(sqfs_dir_reader_t *rd);

/**
 * @brief Navigate a directory reader to the location of a directory
 *        represented by an inode.
 *
 * @memberof sqfs_dir_reader_t
 *
 * This function seeks to the meta data block containing the directory
 * listing that the given inode referes to and resets the internal state.
 * After that, consequtive cals to @ref sqfs_dir_reader_read can be made
 * to iterate over the directory contents.
 *
 * @param rd A pointer to a directory reader.
 * @param inode An directory or extended directory inode.
 *
 * @return Zero on success, an @ref E_SQFS_ERROR value on failure.
 */
SQFS_API int sqfs_dir_reader_open_dir(sqfs_dir_reader_t *rd,
				      const sqfs_inode_generic_t *inode);

/**
 * @brief Reset a directory reader back to the beginning of the listing.
 *
 * @memberof sqfs_dir_reader_t
 *
 * @param rd A pointer to a directory reader.
 *
 * @return Zero on success, an @ref E_SQFS_ERROR value on failure.
 */
SQFS_API int sqfs_dir_reader_rewind(sqfs_dir_reader_t *rd);

/**
 * @brief Seek through the current directory listing to locate an
 *        entry by name.
 *
 * @memberof sqfs_dir_reader_t
 *
 * @param rd A pointer to a directory reader.
 * @param name The name of the entry to find.
 *
 * @return Zero on success, an @ref E_SQFS_ERROR value on failure.
 */
SQFS_API int sqfs_dir_reader_find(sqfs_dir_reader_t *rd, const char *name);

/**
 * @brief Read a directory entry and advance the internal position indicator
 *        to the next one.
 *
 * @memberof sqfs_dir_reader_t
 *
 * Call this function repeatedly to iterate over a directory listing. It
 * returns a positive number to indicate that it couldn't fetch any more data
 * because the end of the listing was reached. A negative value indicates an
 * error.
 *
 * After calling this function, you can use @ref sqfs_dir_reader_get_inode to
 * read the full inode structure that the current entry referes to.
 *
 * @param rd A pointer to a directory reader.
 * @param out Returns a pointer to a directory entry on success that can be
 *            freed with a single free call.
 *
 * @return Zero on success, an @ref E_SQFS_ERROR value on failure, a positive
 *         number if the end of the current directory listing has been reached.
 */
SQFS_API int sqfs_dir_reader_read(sqfs_dir_reader_t *rd,
				  sqfs_dir_entry_t **out);

/**
 * @brief Read the inode that the current directory entry points to.
 *
 * @memberof sqfs_dir_reader_t
 *
 * @param rd A pointer to a directory reader.
 * @param out Returns a pointer to a generic inode that can be freed with a
 *            single free call.
 *
 * @return Zero on success, an @ref E_SQFS_ERROR value on failure.
 */
SQFS_API int sqfs_dir_reader_get_inode(sqfs_dir_reader_t *rd,
				       sqfs_inode_generic_t **inode);

/**
 * @brief Read the root inode using the location given by the super block.
 *
 * @memberof sqfs_dir_reader_t
 *
 * @param rd A pointer to a directory reader.
 * @param out Returns a pointer to a generic inode that can be freed with a
 *            single free call.
 *
 * @return Zero on success, an @ref E_SQFS_ERROR value on failure.
 */
SQFS_API int sqfs_dir_reader_get_root_inode(sqfs_dir_reader_t *rd,
					    sqfs_inode_generic_t **inode);

/**
 * @brief Find an inode through path traversal from the root node downwards.
 *
 * @memberof sqfs_dir_reader_t
 *
 * @param rd A pointer to a directory reader.
 * @param path A path to resolve into an inode. Forward or backward slashes can
 *             be used to seperate path components. Resolving '.' or '..' is
 *             not supported.
 * @param out Returns a pointer to a generic inode that can be freed with a
 *            single free call.
 *
 * @return Zero on success, an @ref E_SQFS_ERROR value on failure.
 */
SQFS_API int sqfs_dir_reader_find_by_path(sqfs_dir_reader_t *rd,
					  const char *path,
					  sqfs_inode_generic_t **out);

#ifdef __cplusplus
}
#endif

#endif /* SQFS_DIR_H */
