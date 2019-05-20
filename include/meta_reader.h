/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef META_READER_H
#define META_READER_H

#include "compress.h"
#include "squashfs.h"

typedef struct meta_reader_t meta_reader_t;

/**
 * @brief Create a meta data reader
 *
 * @memberof meta_reader_t
 *
 * @note This function internally prints error message to stderr on failure
 *
 * @param fd  The underlying file descriptor to read from
 * @param cmp A pointer to a compressor to use for extracting the data
 *
 * @return A pointer to a meta data reader, NULL on failure
 */
meta_reader_t *meta_reader_create(int fd, compressor_t *cmp);

/**
 * @brief Destroy a meta data reader and free all memory used by it
 *
 * @memberof meta_reader_t
 *
 * @param m A pointer to a meta data reader
 */
void meta_reader_destroy(meta_reader_t *m);

/**
 * @brief Seek to a specific meta data block and offset
 *
 * @memberof meta_reader_t
 *
 * @note This function internally prints error message to stderr on failure
 *
 * @param m           A pointer to a meta data reader
 * @param block_start The absolute position of the 16 bit header right before
 *                    the compressed data block
 * @param offset      A byte offset into the data block
 *
 * @return Zero on success, -1 on failure
 */
int meta_reader_seek(meta_reader_t *m, uint64_t block_start,
		     size_t offset);

/**
 * @brief Read a chunk of data from a meta data block
 *
 * @memberof meta_reader_t
 *
 * @note This function internally prints error message to stderr on failure
 *
 * If the end of the block is reached, this function transparently tries to
 * interpret the data after the current block as a further meta data block,
 * i.e. it can transparently read across multiple meta data blocks.
 *
 * @param m    A pointer to a meta data reader
 * @param data A pointer to a memory block to write the data to
 * @param size The number of bytes to read
 *
 * @return Zero on success, -1 on failure
 */
int meta_reader_read(meta_reader_t *m, void *data, size_t size);

/**
 * @brief Read an inode from a meta data block
 *
 * @memberof meta_reader_t
 *
 * @note This function internally prints error message to stderr on failure
 *
 * This function is a conveniance wrapper around @ref meta_reader_read that
 * reads and decodes an entire SquashFS inode. It first reads the common inode
 * header, interprets it and reads the additional, type dependend data.
 *
 * @param ir          A pointer to a meta data reader
 * @param super       A pointer to the SquashFS super block
 * @param block_start A byte offset relative to the inode table start where
 *                    the meta data containing the inode starts
 * @param offset      A byte offset into the uncompressed meta data block
 *                    where the inode is stored
 *
 * @return A pointer to the decoded inode or NULL on failure. Can be freed
 *         with a single free call.
 */
sqfs_inode_generic_t *meta_reader_read_inode(meta_reader_t *ir,
					     sqfs_super_t *super,
					     uint64_t block_start,
					     size_t offset);

/**
 * @brief Read a directory header from a meta data block
 *
 * @memberof meta_reader_t
 *
 * @note This function internally prints error message to stderr on failure
 *
 * This function is a conveniance wrapper around @ref meta_reader_read that
 * reads and decodes a SquashFS directory header.
 *
 * @param m   A pointer to a meta data reader
 * @param hdr A pointer to a directory header structure to write the decoded
 *            data to
 *
 * @return Zero on success, -1 on failure
 */
int meta_reader_read_dir_header(meta_reader_t *m, sqfs_dir_header_t *hdr);

/**
 * @brief Read a directory entry from a meta data block
 *
 * @memberof meta_reader_t
 *
 * @note This function internally prints error message to stderr on failure
 *
 * This function is a conveniance wrapper around @ref meta_reader_read that
 * reads and decodes a SquashFS directory entry.
 *
 * @param m A pointer to a meta data reader
 *
 * @return A pointer to a directory entry or NULL on failure. Can be freed
 *         with a single free call.
 */
sqfs_dir_entry_t *meta_reader_read_dir_ent(meta_reader_t *m);

#endif /* META_READER_H */
