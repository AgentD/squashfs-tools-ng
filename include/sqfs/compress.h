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

typedef struct {
	uint16_t id;
	uint16_t flags;
	uint32_t block_size;

	union {
		struct {
			uint16_t level;
			uint16_t window_size;
		} gzip;

		struct {
			uint16_t level;
		} zstd;

		struct {
			uint16_t algorithm;
			uint16_t level;
		} lzo;

		struct {
			uint32_t dict_size;
		} xz;
	} opt;
} compressor_config_t;

typedef enum {
	SQFS_COMP_FLAG_LZ4_HC = 0x0001,
	SQFS_COMP_FLAG_LZ4_ALL = 0x0001,

	SQFS_COMP_FLAG_XZ_X86 = 0x0001,
	SQFS_COMP_FLAG_XZ_POWERPC = 0x0002,
	SQFS_COMP_FLAG_XZ_IA64 = 0x0004,
	SQFS_COMP_FLAG_XZ_ARM = 0x0008,
	SQFS_COMP_FLAG_XZ_ARMTHUMB = 0x0010,
	SQFS_COMP_FLAG_XZ_SPARC = 0x0020,
	SQFS_COMP_FLAG_XZ_ALL = 0x003F,

	SQFS_COMP_FLAG_GZIP_DEFAULT = 0x0001,
	SQFS_COMP_FLAG_GZIP_FILTERED = 0x0002,
	SQFS_COMP_FLAG_GZIP_HUFFMAN = 0x0004,
	SQFS_COMP_FLAG_GZIP_RLE = 0x0008,
	SQFS_COMP_FLAG_GZIP_FIXED = 0x0010,
	SQFS_COMP_FLAG_GZIP_ALL = 0x001F,

	SQFS_COMP_FLAG_UNCOMPRESS = 0x8000,
	SQFS_COMP_FLAG_GENERIC_ALL = 0x8000,
} SQFS_COMP_FLAG;

typedef enum {
	SQFS_LZO1X_1 = 0,
	SQFS_LZO1X_1_11 = 1,
	SQFS_LZO1X_1_12 = 2,
	SQFS_LZO1X_1_15 = 3,
	SQFS_LZO1X_999	= 4,
} SQFS_LZO_ALGORITHM;

#define SQFS_GZIP_DEFAULT_LEVEL (9)
#define SQFS_GZIP_DEFAULT_WINDOW (15)

#define SQFS_LZO_DEFAULT_ALG SQFS_LZO1X_999
#define SQFS_LZO_DEFAULT_LEVEL (8)

#define SQFS_ZSTD_DEFAULT_LEVEL (15)

#define SQFS_GZIP_MIN_LEVEL (1)
#define SQFS_GZIP_MAX_LEVEL (9)

#define SQFS_LZO_MIN_LEVEL (0)
#define SQFS_LZO_MAX_LEVEL (9)

#define SQFS_ZSTD_MIN_LEVEL (1)
#define SQFS_ZSTD_MAX_LEVEL (22)

#define SQFS_GZIP_MIN_WINDOW (8)
#define SQFS_GZIP_MAX_WINDOW (15)

#ifdef __cplusplus
extern "C" {
#endif

int compressor_config_init(compressor_config_t *cfg, E_SQFS_COMPRESSOR id,
			   size_t block_size, uint16_t flags);

bool compressor_exists(E_SQFS_COMPRESSOR id);

compressor_t *compressor_create(const compressor_config_t *cfg);

const char *compressor_name_from_id(E_SQFS_COMPRESSOR id);

int compressor_id_from_name(const char *name, E_SQFS_COMPRESSOR *out);

#ifdef __cplusplus
}
#endif

#endif /* COMPRESS_H */
