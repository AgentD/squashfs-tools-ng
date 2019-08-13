/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * create_block.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "block_processor.h"
#include "util.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

block_t *create_block(const char *filename, int fd, size_t size,
		      void *user, uint32_t flags)
{
	block_t *blk = calloc(1, sizeof(*blk) + size);

	if (blk == NULL) {
		perror(filename);
		return NULL;
	}

	if (fd >= 0) {
		if (read_data(filename, fd, blk->data, size)) {
			free(blk);
			return NULL;
		}
	}

	blk->size = size;
	blk->user = user;
	blk->flags = flags;
	return blk;
}
