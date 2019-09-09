/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * block_processor.h - This file is part of libsquashfs
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
#ifndef SFQS_BLOCK_PROCESSOR_H
#define SFQS_BLOCK_PROCESSOR_H

#include "sqfs/predef.h"

enum {
	/* only calculate checksum, do NOT compress the data */
	SQFS_BLK_DONT_COMPRESS = 0x0001,

	/* set by compressor worker if the block was actually compressed */
	SQFS_BLK_IS_COMPRESSED = 0x0002,

	/* do not calculate block checksum */
	SQFS_BLK_DONT_CHECKSUM = 0x0004,

	/* set by compressor worker if compression failed */
	SQFS_BLK_COMPRESS_ERROR = 0x0008,

	/* first user setable block flag */
	SQFS_BLK_USER = 0x0080
};

struct sqfs_block_t {
	/* used internally, ignored and overwritten when enqueueing blocks */
	sqfs_block_t *next;
	uint32_t sequence_number;

	/* Size of the data area */
	uint32_t size;

	/* checksum of the input data */
	uint32_t checksum;

	/* user settable file block index */
	uint32_t index;

	/* user pointer associated with the block */
	void *user;

	/* user settable flag field */
	uint32_t flags;

	/* raw data to be processed */
	uint8_t data[];
};

/*
  Gets called for each processed block. May be called from a different thread
  than the one that calls enqueue, but only from one thread at a time.
  Guaranteed to be called on blocks in the order that they are submitted
  to enqueue.

  A non-zero return value is interpreted as fatal error.
 */
typedef int (*sqfs_block_cb)(void *user, sqfs_block_t *blk);

#ifdef __cplusplus
extern "C" {
#endif

SQFS_API
sqfs_block_processor_t *sqfs_block_processor_create(size_t max_block_size,
						    sqfs_compressor_t *cmp,
						    unsigned int num_workers,
						    void *user,
						    sqfs_block_cb callback);

SQFS_API void sqfs_block_processor_destroy(sqfs_block_processor_t *proc);

/*
  Add a block to be processed. Returns non-zero on error and prints a message
  to stderr.

  The function takes over ownership of the submitted block. It is freed with
  a after processing and calling the block callback.

  Even on failure, the workers may still be running and
  sqfs_block_processor_finish must be called before cleaning up.
*/
SQFS_API int sqfs_block_processor_enqueue(sqfs_block_processor_t *proc,
					  sqfs_block_t *block);

/*
  Wait for the compressor workers to finish. Returns zero on success, non-zero
  if an internal error occoured or one of the block callbacks returned a
  non-zero value.
 */
SQFS_API int sqfs_block_processor_finish(sqfs_block_processor_t *proc);

/*
  Convenience function to process a data block. Returns 0 on success,
  prints to stderr on failure.
 */
SQFS_API int sqfs_block_process(sqfs_block_t *block, sqfs_compressor_t *cmp,
				uint8_t *scratch, size_t scratch_size);

#ifdef __cplusplus
}
#endif

#endif /* SFQS_BLOCK_PROCESSOR_H */
