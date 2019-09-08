/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * write_table.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "config.h"

#include "sqfs/meta_writer.h"
#include "sqfs/error.h"
#include "sqfs/super.h"
#include "sqfs/table.h"
#include "sqfs/data.h"
#include "sqfs/io.h"
#include "util.h"

#include <endian.h>
#include <stdlib.h>

int sqfs_write_table(sqfs_file_t *file, sqfs_super_t *super,
		     sqfs_compressor_t *cmp, const void *data,
		     size_t table_size, uint64_t *start)
{
	size_t block_count, list_size, diff, blkidx = 0;
	uint64_t off, block, *locations;
	sqfs_meta_writer_t *m;
	uint32_t offset;
	int ret;

	block_count = table_size / SQFS_META_BLOCK_SIZE;
	if ((table_size % SQFS_META_BLOCK_SIZE) != 0)
		++block_count;

	locations = alloc_array(sizeof(uint64_t), block_count);

	if (locations == NULL)
		return SQFS_ERROR_ALLOC;

	/* Write actual data */
	m = sqfs_meta_writer_create(file, cmp, false);
	if (m == NULL) {
		ret = SQFS_ERROR_ALLOC;
		goto out_idx;
	}

	while (table_size > 0) {
		sqfs_meta_writer_get_position(m, &block, &offset);
		locations[blkidx++] = htole64(super->bytes_used + block);

		diff = SQFS_META_BLOCK_SIZE;
		if (diff > table_size)
			diff = table_size;

		ret = sqfs_meta_writer_append(m, data, diff);
		if (ret)
			goto out;

		data = (const char *)data + diff;
		table_size -= diff;
	}

	ret = sqfs_meta_writer_flush(m);
	if (ret)
		goto out;

	sqfs_meta_writer_get_position(m, &block, &offset);
	super->bytes_used += block;

	/* write location list */
	*start = super->bytes_used;

	list_size = sizeof(uint64_t) * block_count;

	off = file->get_size(file);

	ret = file->write_at(file, off, locations, list_size);
	if (ret)
		goto out;

	super->bytes_used += list_size;

	/* cleanup */
	ret = 0;
out:
	sqfs_meta_writer_destroy(m);
out_idx:
	free(locations);
	return ret;
}
