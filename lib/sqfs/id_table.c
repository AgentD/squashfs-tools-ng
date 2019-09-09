/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * id_table.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "config.h"

#include "sqfs/id_table.h"
#include "sqfs/super.h"
#include "sqfs/table.h"
#include "sqfs/error.h"

#include <stdlib.h>
#include <string.h>

struct sqfs_id_table_t {
	uint32_t *ids;
	size_t num_ids;
	size_t max_ids;
};

sqfs_id_table_t *sqfs_id_table_create(void)
{
	return calloc(1, sizeof(sqfs_id_table_t));
}

void sqfs_id_table_destroy(sqfs_id_table_t *tbl)
{
	free(tbl->ids);
	free(tbl);
}

int sqfs_id_table_id_to_index(sqfs_id_table_t *tbl, uint32_t id, uint16_t *out)
{
	size_t i, sz;
	void *ptr;

	for (i = 0; i < tbl->num_ids; ++i) {
		if (tbl->ids[i] == id) {
			*out = i;
			return 0;
		}
	}

	if (tbl->num_ids == 0x10000)
		return SQFS_ERROR_OVERFLOW;

	if (tbl->num_ids == tbl->max_ids) {
		sz = (tbl->max_ids ? tbl->max_ids * 2 : 16);
		ptr = realloc(tbl->ids, sizeof(tbl->ids[0]) * sz);

		if (ptr == NULL)
			return SQFS_ERROR_ALLOC;

		tbl->ids = ptr;
		tbl->max_ids = sz;
	}

	*out = tbl->num_ids;
	tbl->ids[tbl->num_ids++] = id;
	return 0;
}

int sqfs_id_table_index_to_id(const sqfs_id_table_t *tbl, uint16_t index,
			      uint32_t *out)
{
	if (index >= tbl->num_ids)
		return SQFS_ERROR_OUT_OF_BOUNDS;

	*out = tbl->ids[index];
	return 0;
}

int sqfs_id_table_read(sqfs_id_table_t *tbl, sqfs_file_t *file,
		       sqfs_super_t *super, sqfs_compressor_t *cmp)
{
	uint64_t upper_limit, lower_limit;
	void *raw_ids;
	size_t i;
	int ret;

	if (tbl->ids != NULL) {
		free(tbl->ids);
		tbl->num_ids = 0;
		tbl->max_ids = 0;
		tbl->ids = NULL;
	}

	if (!super->id_count || super->id_table_start >= super->bytes_used)
		return SQFS_ERROR_CORRUPTED;

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
	ret = sqfs_read_table(file, cmp, tbl->num_ids * sizeof(uint32_t),
			      super->id_table_start, lower_limit,
			      upper_limit, &raw_ids);
	if (ret)
		return ret;

	tbl->ids = raw_ids;

	for (i = 0; i < tbl->num_ids; ++i)
		tbl->ids[i] = le32toh(tbl->ids[i]);

	return 0;
}

int sqfs_id_table_write(sqfs_id_table_t *tbl, sqfs_file_t *file,
			sqfs_super_t *super, sqfs_compressor_t *cmp)
{
	uint64_t start;
	size_t i;
	int ret;

	for (i = 0; i < tbl->num_ids; ++i)
		tbl->ids[i] = htole32(tbl->ids[i]);

	super->id_count = tbl->num_ids;

	ret = sqfs_write_table(file, cmp, tbl->ids,
			       sizeof(tbl->ids[0]) * tbl->num_ids, &start);

	super->id_table_start = start;

	for (i = 0; i < tbl->num_ids; ++i)
		tbl->ids[i] = le32toh(tbl->ids[i]);

	return ret;
}
