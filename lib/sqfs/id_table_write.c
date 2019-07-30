/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * id_table_write.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "highlevel.h"

int id_table_write(id_table_t *tbl, int outfd, sqfs_super_t *super,
		   compressor_t *cmp)
{
	uint64_t start;
	size_t i;
	int ret;

	for (i = 0; i < tbl->num_ids; ++i)
		tbl->ids[i] = htole32(tbl->ids[i]);

	super->id_count = tbl->num_ids;

	ret = sqfs_write_table(outfd, super, cmp, tbl->ids,
			       sizeof(tbl->ids[0]) * tbl->num_ids, &start);

	super->id_table_start = start;

	for (i = 0; i < tbl->num_ids; ++i)
		tbl->ids[i] = le32toh(tbl->ids[i]);

	return ret;
}
