/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * statistics.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "common.h"

#include <stdio.h>

static void post_block_write(void *user, const sqfs_block_t *block,
			     sqfs_file_t *file)
{
	data_writer_stats_t *stats = user;
	(void)file;

	if (block->size == 0)
		return;

	if (block->flags & SQFS_BLK_FRAGMENT_BLOCK) {
		stats->frag_blocks_written += 1;
	} else {
		stats->blocks_written += 1;
	}

	stats->bytes_written += block->size;
}

static void pre_fragment_store(void *user, sqfs_block_t *block)
{
	data_writer_stats_t *stats = user;
	(void)block;

	stats->frag_count += 1;
}

static void notify_blocks_erased(void *user, size_t count, sqfs_u64 bytes)
{
	data_writer_stats_t *stats = user;

	stats->bytes_written -= bytes;
	stats->blocks_written -= count;
	stats->duplicate_blocks += count;
}

static void notify_fragment_discard(void *user, const sqfs_block_t *block)
{
	data_writer_stats_t *stats = user;
	(void)block;

	stats->frag_dup += 1;
}

static const sqfs_block_hooks_t hooks = {
	.size = sizeof(hooks),
	.post_block_write = post_block_write,
	.pre_fragment_store = pre_fragment_store,
	.notify_blocks_erased = notify_blocks_erased,
	.notify_fragment_discard = notify_fragment_discard,
};

void register_stat_hooks(sqfs_data_writer_t *data, data_writer_stats_t *stats)
{
	sqfs_data_writer_set_hooks(data, stats, &hooks);
}

void sqfs_print_statistics(sqfs_super_t *super, data_writer_stats_t *stats)
{
	size_t ratio;

	if (stats->bytes_written > 0) {
		ratio = (100 * stats->bytes_written) / stats->bytes_read;
	} else {
		ratio = 100;
	}

	fputs("---------------------------------------------------\n", stdout);
	printf("Input files processed: %zu\n", stats->file_count);
	printf("Data blocks actually written: %zu\n", stats->blocks_written);
	printf("Fragment blocks written: %zu\n", stats->frag_blocks_written);
	printf("Duplicate data blocks omitted: %zu\n", stats->duplicate_blocks);
	printf("Sparse blocks omitted: %zu\n", stats->sparse_blocks);
	printf("Fragments actually written: %zu\n", stats->frag_count);
	printf("Duplicated fragments omitted: %zu\n", stats->frag_dup);
	printf("Total number of inodes: %u\n", super->inode_count);
	printf("Number of unique group/user IDs: %u\n", super->id_count);
	printf("Data compression ratio: %zu%%\n", ratio);
}
