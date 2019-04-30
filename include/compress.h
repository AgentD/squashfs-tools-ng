/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef COMPRESS_H
#define COMPRESS_H

#include <sys/types.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "squashfs.h"

typedef struct compressor_t compressor_t;

struct compressor_t {
	ssize_t (*do_block)(compressor_t *cmp, uint8_t *block, size_t outsize);

	void (*destroy)(compressor_t *stream);
};

bool compressor_exists(E_SQFS_COMPRESSOR id);

compressor_t *compressor_create(E_SQFS_COMPRESSOR id, bool compress,
				size_t block_size);

#endif /* COMPRESS_H */
