/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * id_table_read.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "highlevel.h"
#include "util.h"

#include <stdlib.h>
#include <stdio.h>

int id_table_read(id_table_t *tbl, int fd, sqfs_super_t *super,
		  compressor_t *cmp)
{
	uint64_t upper_limit, lower_limit;
	size_t i;

	if (tbl->ids != NULL) {
		free(tbl->ids);
		tbl->num_ids = 0;
		tbl->max_ids = 0;
		tbl->ids = NULL;
	}

	if (!super->id_count || super->id_table_start >= super->bytes_used) {
		fputs("ID table missing from file system\n", stderr);
		return -1;
	}

	upper_limit = super->id_table_start;
	lower_limit = super->directory_table_start;

	if (super->fragment_table_start > lower_limit &&
	    super->fragment_table_start < upper_limit) {
		lower_limit = super->fragment_table_start;
	}

	if (super->export_table_start > lower_limit &&
	    super->export_table_start < upper_limit) {
		lower_limit = super->export_table_start;
	}

	tbl->num_ids = super->id_count;
	tbl->max_ids = super->id_count;
	tbl->ids = sqfs_read_table(fd, cmp, tbl->num_ids * sizeof(uint32_t),
				   super->id_table_start, lower_limit,
				   upper_limit);
	if (tbl->ids == NULL)
		return -1;

	for (i = 0; i < tbl->num_ids; ++i)
		tbl->ids[i] = le32toh(tbl->ids[i]);

	return 0;
}
