/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * statistics.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "highlevel.h"

#include <stdio.h>

void sqfs_print_statistics(fstree_t *fs, sqfs_super_t *super)
{
	size_t blocks_written = 0, duplicate_blocks = 0, sparse_blocks = 0;
	size_t ratio, file_count = 0, file_dup_count = 0;
	size_t frag_count = 0, frag_dup = 0;
	size_t i, num_blocks, sparse;
	uint64_t output_bytes = 0;
	uint64_t input_bytes = 0;
	file_info_t *fi;
	bool is_dupe;

	for (fi = fs->files; fi != NULL; fi = fi->next) {
		num_blocks = fi->size / fs->block_size;
		is_dupe = true;

		if ((fi->size % fs->block_size) &&
		    !(fi->flags & FILE_FLAG_HAS_FRAGMENT)) {
			++num_blocks;
		}

		for (sparse = 0, i = 0; i < num_blocks; ++i) {
			if (fi->block_size[i] == 0)
				sparse += 1;
		}

		if (num_blocks > sparse) {
			if (fi->flags & FILE_FLAG_BLOCKS_ARE_DUPLICATE) {
				duplicate_blocks += num_blocks - sparse;
			} else {
				blocks_written += num_blocks - sparse;
				is_dupe = false;
			}
		}

		if (fi->flags & FILE_FLAG_HAS_FRAGMENT) {
			if (fi->flags & FILE_FLAG_FRAGMENT_IS_DUPLICATE) {
				frag_dup += 1;
			} else {
				frag_count += 1;
				is_dupe = false;
			}
		}

		if (is_dupe)
			file_dup_count += 1;

		sparse_blocks += sparse;
		file_count += 1;
		input_bytes += fi->size;
	}

	if (input_bytes > 0) {
		output_bytes = super->inode_table_start - sizeof(*super);
		ratio = (100 * output_bytes) / input_bytes;
	} else {
		ratio = 100;
	}

	fputs("---------------------------------------------------\n", stdout);
	printf("Input files processed: %zu\n", file_count);
	printf("Files that were complete duplicates: %zu\n", file_dup_count);
	printf("Data blocks actually written: %zu\n", blocks_written);
	printf("Fragment blocks written: %u\n", super->fragment_entry_count);
	printf("Duplicate data blocks omitted: %zu\n", duplicate_blocks);
	printf("Sparse blocks omitted: %zu\n", sparse_blocks);
	printf("Fragments actually written: %zu\n", frag_count);
	printf("Duplicated fragments omitted: %zu\n", frag_dup);
	printf("Total number of inodes: %u\n", super->inode_count);
	printf("Number of unique group/user IDs: %u\n", super->id_count);
	printf("Data compression ratio: %zu%%\n", ratio);
}
