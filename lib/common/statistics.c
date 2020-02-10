/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * statistics.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "common.h"

#include <stdio.h>

void sqfs_print_statistics(const sqfs_super_t *super,
			   const sqfs_block_processor_t *blk,
			   const sqfs_block_writer_t *wr)
{
	const sqfs_block_processor_stats_t *proc_stats;
	const sqfs_block_writer_stats_t *wr_stats;
	size_t ratio;

	proc_stats = sqfs_block_processor_get_stats(blk);
	wr_stats = sqfs_block_writer_get_stats(wr);

	if (proc_stats->input_bytes_read > 0) {
		ratio = (100 * wr_stats->bytes_written) /
			proc_stats->input_bytes_read;
	} else {
		ratio = 100;
	}

	fputs("---------------------------------------------------\n", stdout);
	printf("Input files processed: %lu\n", proc_stats->input_bytes_read);
	printf("Data blocks actually written: %lu\n", wr_stats->blocks_written);
	printf("Fragment blocks written: %lu\n", proc_stats->frag_block_count);
	printf("Duplicate data blocks omitted: %lu\n",
	       wr_stats->blocks_submitted - wr_stats->blocks_written);
	printf("Sparse blocks omitted: %lu\n", proc_stats->sparse_block_count);
	printf("Fragments actually written: %lu\n",
	       proc_stats->actual_frag_count);
	printf("Duplicated fragments omitted: %lu\n",
	       proc_stats->total_frag_count - proc_stats->actual_frag_count);
	printf("Total number of inodes: %u\n", super->inode_count);
	printf("Number of unique group/user IDs: %u\n", super->id_count);
	printf("Data compression ratio: " PRI_SZ "%%\n", ratio);
}
