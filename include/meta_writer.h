/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef META_WRITER_H
#define META_WRITER_H

#include "compress.h"
#include "squashfs.h"

/**
 * @struct meta_writer_t
 *
 * @brief Abstracts away I/O on SquashFS meta data
 */
typedef struct {
	/**
	 * @brief A byte offset into the uncompressed data of the current block
	 */
	size_t offset;

	/**
	 * @brief The location of the current block in the file
	 */
	size_t block_offset;

	/**
	 * @brief The underlying file descriptor to write to
	 */
	int outfd;

	/**
	 * @brief A pointer to the compressor to use for compressing the data
	 */
	compressor_t *cmp;

	/**
	 * @brief The raw data chunk that data is appended to
	 */
	uint8_t data[SQFS_META_BLOCK_SIZE + 2];

	/**
	 * @brief Scratch buffer for compressing data
	 */
	uint8_t scratch[SQFS_META_BLOCK_SIZE + 2];
} meta_writer_t;

/**
 * @brief Create a meta data writer
 *
 * @memberof meta_writer_t
 *
 * @note This function internally prints error message to stderr on failure
 *
 * @param fd  The underlying file descriptor to write from
 * @param cmp A pointer to a compressor to use for compressing the data
 *
 * @return A pointer to a meta data writer, NULL on failure
 */
meta_writer_t *meta_writer_create(int fd, compressor_t *cmp);

/**
 * @brief Destroy a meta data writer and free all memory used by it
 *
 * @memberof meta_writer_t
 *
 * @param m A pointer to a meta data reader
 */
void meta_writer_destroy(meta_writer_t *m);

/**
 * @brief Flush the currently unfinished meta data block to disk
 *
 * @memberof meta_writer_t
 *
 * @note This function internally prints error message to stderr on failure
 *
 * If data has been collected in the block buffer but it is not complete yet,
 * this function tries to compress it and write it out anyway and reset the
 * internal counters.
 *
 * @param m A pointer to a meta data reader
 *
 * @return Zero on success, -1 on failure
 */
int meta_writer_flush(meta_writer_t *m);

/**
 * @brief Append data to the current meta data block
 *
 * @memberof meta_writer_t
 *
 * @note This function internally prints error message to stderr on failure
 *
 * This function appends the input data to an internal meta data buffer. If
 * the internal buffer is full, it is compressed and written to disk using
 * @ref meta_writer flush, i.e. the function allows for transparent writing
 * across meta data blocks.
 *
 * @param m    A pointer to a meta data reader
 * @param data A pointer to the data block to append
 * @param size The number of bytes to read from the data blob
 *
 * @return Zero on success, -1 on failure
 */
int meta_writer_append(meta_writer_t *m, const void *data, size_t size);

#endif /* META_WRITER_H */
