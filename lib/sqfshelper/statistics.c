/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * statistics.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "highlevel.h"

#include <stdio.h>

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
