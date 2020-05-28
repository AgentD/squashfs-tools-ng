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
	sqfs_u64 bytes_written, blocks_written;
	char read_sz[32], written_sz[32];
	size_t ratio;

	proc_stats = sqfs_block_processor_get_stats(blk);
	blocks_written = wr->get_block_count(wr);

	bytes_written = super->inode_table_start - sizeof(*super);

	if (proc_stats->input_bytes_read > 0) {
		ratio = (100 * bytes_written) / proc_stats->input_bytes_read;
	} else {
		ratio = 100;
	}

	print_size(proc_stats->input_bytes_read, read_sz, false);
	print_size(bytes_written, written_sz, false);

	fputs("---------------------------------------------------\n", stdout);
	printf("Data bytes read: %s\n", read_sz);
	printf("Data bytes written: %s\n", written_sz);
	printf("Data compression ratio: " PRI_SZ "%%\n", ratio);
	fputc('\n', stdout);

	printf("Data blocks written: " PRI_U64 "\n", blocks_written);
	printf("Out of which where fragment blocks: " PRI_U64 "\n",
	       proc_stats->frag_block_count);

	printf("Duplicate blocks omitted: " PRI_U64 "\n",
	       proc_stats->data_block_count + proc_stats->frag_block_count -
	       blocks_written);

	printf("Sparse blocks omitted: " PRI_U64 "\n",
	       proc_stats->sparse_block_count);
	fputc('\n', stdout);

	printf("Fragments actually written: " PRI_U64 "\n",
	       proc_stats->actual_frag_count);
	printf("Duplicated fragments omitted: " PRI_U64 "\n",
	       proc_stats->total_frag_count - proc_stats->actual_frag_count);
	printf("Total number of inodes: %u\n", super->inode_count);
	printf("Number of unique group/user IDs: %u\n", super->id_count);
	fputc('\n', stdout);
}
