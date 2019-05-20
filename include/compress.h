/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef COMPRESS_H
#define COMPRESS_H

#include <sys/types.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "squashfs.h"

typedef struct compressor_t compressor_t;

/* Encapsultes a compressor with a simple interface to compress or
   uncompress/extract blocks of data. */
struct compressor_t {
	/* Write compressor options to the output file if necessary.
	   Returns the number of bytes written or -1 on failure.
	   Internally prints error messages to stderr. */
	int (*write_options)(compressor_t *cmp, int fd);

	/* Read compressor options to the input file.
	   Returns zero on success, -1 on failure.
	   Internally prints error messages to stderr. */
	int (*read_options)(compressor_t *cmp, int fd);

	/*
	  Compress or uncompress a chunk of data.

	  Returns the number of bytes written to the output buffer, -1 on
	  failure or 0 if the output buffer was too small.
	  The compressor also returns 0 if the compressed result ends
	  up larger than the original input.

	  Internally prints compressor specific error messages to stderr.
	*/
	ssize_t (*do_block)(compressor_t *cmp, const uint8_t *in, size_t size,
			    uint8_t *out, size_t outsize);

	void (*destroy)(compressor_t *stream);
};

bool compressor_exists(E_SQFS_COMPRESSOR id);

/* block_size is the configured block size for the SquashFS image. Needed
   by some compressors to set internal defaults. */
compressor_t *compressor_create(E_SQFS_COMPRESSOR id, bool compress,
				size_t block_size);

#endif /* COMPRESS_H */
