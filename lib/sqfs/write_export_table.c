/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"

#include "highlevel.h"

#include <stdlib.h>
#include <stdio.h>

int write_export_table(int outfd, fstree_t *fs, sqfs_super_t *super,
		       compressor_t *cmp)
{
	uint64_t *table, start;
	size_t i;
	int ret;

	table = malloc(sizeof(uint64_t) * (fs->inode_tbl_size - 1));

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

	ret = sqfs_write_table(outfd, super, table, sizeof(table[0]),
			       fs->inode_tbl_size - 1, &start, cmp);

	super->export_table_start = start;
	super->flags |= SQFS_FLAG_EXPORTABLE;
	free(table);
	return ret;
}
