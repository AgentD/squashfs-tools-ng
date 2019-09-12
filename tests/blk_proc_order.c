/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * add_by_path.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "sqfs/block_processor.h"
#include "sqfs/compress.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#ifdef HAVE_PTHREAD
#include <pthread.h>
#endif

static ssize_t dummy_do_block(sqfs_compressor_t *cmp, const uint8_t *in,
			      size_t size, uint8_t *out, size_t outsize)
{
	int delay, result;
	(void)cmp; (void)size; (void)outsize;

	delay = ((int *)in)[0];
	result = ((int *)in)[1];

	memcpy(out, in, result);
	sleep(delay);
	return (size_t)result >= size ? 0 : result;
}

static sqfs_compressor_t *dummy_create_copy(sqfs_compressor_t *cmp)
{
	sqfs_compressor_t *cpy = malloc(sizeof(*cpy));
	assert(cpy != NULL);
	*cpy = *cmp;
	return cpy;
}

static void dummy_destroy(sqfs_compressor_t *cmp)
{
	free(cmp);
}

static sqfs_compressor_t *dummy_create(void)
{
	sqfs_compressor_t *cmp = calloc(1, sizeof(*cmp));
	assert(cmp != NULL);

	cmp->do_block = dummy_do_block;
	cmp->create_copy = dummy_create_copy;
	cmp->destroy = dummy_destroy;

	return cmp;
}

static unsigned int blk_index = 0;

#ifdef HAVE_PTHREAD
static pthread_t main_thread;
#endif

static int block_callback(void *user, sqfs_block_t *blk)
{
	assert(main_thread == pthread_self());
	assert(blk->index == blk_index++);

	if (blk->index == 4) {
		assert(blk->size == 4 * sizeof(int));
		assert(blk->flags & SQFS_BLK_DONT_COMPRESS);
		assert(blk->flags & SQFS_BLK_DONT_CHECKSUM);
		assert(!(blk->flags & SQFS_BLK_IS_COMPRESSED));
	} else {
		if (blk->index & 0x01) {
			assert(!(blk->flags & SQFS_BLK_IS_COMPRESSED));
			assert(blk->size == 4 * sizeof(int));
		} else {
			assert(blk->flags & SQFS_BLK_IS_COMPRESSED);
			assert(blk->size == 2 * sizeof(int));
		}
	}

	(void)user;
	return 0;
}

int main(void)
{
	sqfs_compressor_t *cmp = dummy_create();
	sqfs_block_processor_t *proc;
	sqfs_block_t *blk;
	int i;

#ifdef HAVE_PTHREAD
	main_thread = pthread_self();
#endif

	proc = sqfs_block_processor_create(4 * sizeof(int), cmp, 4, 10,
					   NULL, block_callback);
	assert(proc != NULL);

	for (i = 0; i < 4; ++i) {
		blk = calloc(1, sizeof(*blk) + 4 * sizeof(int));
		assert(blk != NULL);
		blk->size = 4 * sizeof(int);
		blk->index = i;
		((int *)blk->data)[0] = 4 - i;
		((int *)blk->data)[1] = (i & 0x01 ? 4 : 2) * sizeof(int);

		assert(sqfs_block_processor_enqueue(proc, blk) == 0);
	}

	blk = calloc(1, sizeof(*blk) + 4 * sizeof(int));
	assert(blk != NULL);
	blk->size = 4 * sizeof(int);
	blk->index = i;
	blk->flags |= SQFS_BLK_DONT_COMPRESS | SQFS_BLK_DONT_CHECKSUM;
	((int *)blk->data)[0] = 0;
	((int *)blk->data)[1] = (i & 0x01 ? 4 : 2) * sizeof(int);
	assert(sqfs_block_processor_enqueue(proc, blk) == 0);

	assert(sqfs_block_processor_finish(proc) == 0);

	sqfs_block_processor_destroy(proc);
	cmp->destroy(cmp);
	return EXIT_SUCCESS;
}
