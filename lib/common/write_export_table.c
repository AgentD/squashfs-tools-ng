/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * write_export_table.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "common.h"

#include <stdlib.h>
#include <stdio.h>

int write_export_table(const char *filename, sqfs_file_t *file,
		       fstree_t *fs, sqfs_super_t *super,
		       sqfs_compressor_t *cmp)
{
	sqfs_u64 *table, start;
	size_t i, size;
	int ret;

	if (fs->inode_tbl_size < 1)
		return 0;

	table = calloc(1, sizeof(sqfs_u64) * fs->inode_tbl_size);

	if (table == NULL) {
		perror("Allocating NFS export table");
		return -1;
	}

	for (i = 0; i < fs->inode_tbl_size; ++i) {
		table[i] = htole64(fs->inode_table[i]->inode_ref);
	}

	size = sizeof(sqfs_u64) * fs->inode_tbl_size;
	ret = sqfs_write_table(file, cmp, table, size, &start);
	if (ret)
		sqfs_perror(filename, "writing NFS export table", ret);

	super->export_table_start = start;
	super->flags |= SQFS_FLAG_EXPORTABLE;
	free(table);
	return ret;
}
