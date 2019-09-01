/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * block_processor.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef BLOCK_PROCESSOR_H
#define BLOCK_PROCESSOR_H

#include "config.h"
#include "sqfs/compress.h"

enum {
	/* only calculate checksum, do NOT compress the data */
	BLK_DONT_COMPRESS = 0x0001,

	/* set by compressor worker if the block was actually compressed */
	BLK_IS_COMPRESSED = 0x0002,

	/* do not calculate block checksum */
	BLK_DONT_CHECKSUM = 0x0004,

	/* set by compressor worker if compression failed */
	BLK_COMPRESS_ERROR = 0x0008,

	/* first user setable block flag */
	BLK_USER = 0x0080
};

typedef struct block_t {
	/* used internally, ignored and overwritten when enqueueing blocks */
	struct block_t *next;
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
} block_t;

typedef struct block_processor_t block_processor_t;

/*
  Gets called for each processed block. May be called from a different thread
  than the one that calls enqueue, but only from one thread at a time.
  Guaranteed to be called on blocks in the order that they are submitted
  to enqueue.

  A non-zero return value is interpreted as fatal error.
 */
typedef int (*block_cb)(void *user, block_t *blk);

block_processor_t *block_processor_create(size_t max_block_size,
					  compressor_t *cmp,
					  unsigned int num_workers,
					  void *user,
					  block_cb callback);

void block_processor_destroy(block_processor_t *proc);

/*
  Add a block to be processed. Returns non-zero on error and prints a message
  to stderr.

  The function takes over ownership of the submitted block. It is freed with
  a after processing and calling the block callback.

  Even on failure, the workers may still be running and
  block_processor_finish must be called before cleaning up.
*/
int block_processor_enqueue(block_processor_t *proc, block_t *block);

/*
  Wait for the compressor workers to finish. Returns zero on success, non-zero
  if an internal error occoured or one of the block callbacks returned a
  non-zero value.
 */
int block_processor_finish(block_processor_t *proc);

/*
  Convenience function to create a block structure and optionally fill it with
  content.

  filename is used for printing error messages. If fd is a valid file
  descriptor (>= 0), the function attempts to populate the payload data
  from the input file.
 */
block_t *create_block(const char *filename, int fd, size_t size,
		      void *user, uint32_t flags);

/*
  Convenience function to process a data block. Returns 0 on success,
  prints to stderr on failure.
 */
int process_block(block_t *block, compressor_t *cmp,
		  uint8_t *scratch, size_t scratch_size);

#endif /* BLOCK_PROCESSOR_H */
