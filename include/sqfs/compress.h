/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * compress.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef COMPRESS_H
#define COMPRESS_H

#include "config.h"

#include <sys/types.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "sqfs/super.h"

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

	/* create another compressor just like this one, i.e.
	   with the exact same settings */
	compressor_t *(*create_copy)(compressor_t *cmp);

	void (*destroy)(compressor_t *stream);
};

#ifdef __cplusplus
extern "C" {
#endif

bool compressor_exists(E_SQFS_COMPRESSOR id);

/* block_size is the configured block size for the SquashFS image. Needed
   by some compressors to set internal defaults. */
compressor_t *compressor_create(E_SQFS_COMPRESSOR id, bool compress,
				size_t block_size, char *options);

void compressor_print_help(E_SQFS_COMPRESSOR id);

void compressor_print_available(void);

E_SQFS_COMPRESSOR compressor_get_default(void);

const char *compressor_name_from_id(E_SQFS_COMPRESSOR id);

int compressor_id_from_name(const char *name, E_SQFS_COMPRESSOR *out);

#ifdef __cplusplus
}
#endif

#endif /* COMPRESS_H */
