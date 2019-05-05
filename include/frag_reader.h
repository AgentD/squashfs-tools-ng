/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef FRAG_READER_H
#define FRAG_READER_H

#include "squashfs.h"
#include "compress.h"

#include <stdint.h>
#include <stddef.h>

/**
 * @struct frag_reader_t
 *
 * @brief A simple interface for accessing fragments in a SquashFS image
 */
typedef struct {
	sqfs_fragment_t *tbl;
	size_t num_fragments;

	int fd;
	compressor_t *cmp;
	size_t block_size;
	size_t used;
	size_t current_index;
	uint8_t buffer[];
} frag_reader_t;

/**
 * @brief Create a fragment reader
 *
 * @memberof frag_reader_t
 *
 * This function internally reads and decodes the fragment table from the
 * image and prints error messages to stderr on the way if it fails.
 *
 * @param super A pointer to the SquashFS super block read from the image
 * @param fd    A file descriptor of opened the SquashFS image
 * @param cmp   A pointer to a compressor object to be used for reading meta
 *              data blocks from the image.
 *
 * @return A pointer to a new fragment reader or NULL on failure
 */
frag_reader_t *frag_reader_create(sqfs_super_t *super, int fd,
				  compressor_t *cmp);

/**
 * @brief Destroy a fragment reader and free all memory it uses
 *
 * @memberof frag_reader_t
 *
 * @param f A pointer to a fragment reader object
 */
void frag_reader_destroy(frag_reader_t *f);

/**
 * @brief Read tail-end packed data from a fragment
 *
 * @memberof frag_reader_t
 *
 * This function internally takes care of loading and uncompressing the
 * fragment block (which is skipped if it has already been loaded earlier).
 * It prints error messages to stderr on the way if it fails, including such
 * errors as trying to index beyond the fragment table.
 *
 * @param f      A pointer to a fragment reader object
 * @param index  A fragment index such as stored in an inode
 * @param offset A byte offset into the fragment block addressed by the index
 * @param buffer A pointer to a destination buffer to copy decoded data to
 * @param size   The number of bytes to copy into the destination buffer
 *
 * @return Zero on success, -1 on failure
 */
int frag_reader_read(frag_reader_t *f, size_t index, size_t offset,
		     void *buffer, size_t size);

#endif /* FRAG_READER_H */
