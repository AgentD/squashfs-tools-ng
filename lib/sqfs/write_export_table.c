/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * write_export_table.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "highlevel.h"
#include "util.h"

#include <stdlib.h>
#include <stdio.h>

int write_export_table(int outfd, fstree_t *fs, sqfs_super_t *super,
		       compressor_t *cmp)
{
	uint64_t *table, start;
	size_t i, size;
	int ret;

	if (fs->inode_tbl_size < 1)
		return 0;

	table = alloc_array(sizeof(uint64_t), (fs->inode_tbl_size - 1));

	if (table == NULL) {
		perror("Allocating NFS export table");
		return -1;
	}

	for (i = 1; i < fs->inode_tbl_size; ++i) {
		if (fs->inode_table[i] == NULL) {
			table[i - 1] = htole64(0xFFFFFFFFFFFFFFFF);
		} else {
			table[i - 1] = htole64(fs->inode_table[i]->inode_ref);
		}
	}

	size = sizeof(uint64_t) * (fs->inode_tbl_size - 1);
	ret = sqfs_write_table(outfd, super, cmp, table, size, &start);

	super->export_table_start = start;
	super->flags |= SQFS_FLAG_EXPORTABLE;
	free(table);
	return ret;
}
