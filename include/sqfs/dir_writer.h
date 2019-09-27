/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * dir_writer.h - This file is part of libsquashfs
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
#ifndef SQFS_DIR_WRITER_H
#define SQFS_DIR_WRITER_H

#include "sqfs/predef.h"

/**
 * @file dir_writer.h
 *
 * @brief Contains declarations for the @ref sqfs_dir_writer_t.
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
				       sqfs_u32 inode_num, sqfs_u64 inode_ref,
				       sqfs_u16 mode);

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
SQFS_API sqfs_u64
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
			      sqfs_u32 xattr, sqfs_u32 parent_ino);

#ifdef __cplusplus
}
#endif

#endif /* SQFS_DIR_WRITER_H */
