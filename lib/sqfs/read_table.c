/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * read_table.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "sqfs/meta_reader.h"
#include "highlevel.h"
#include "util.h"

#include <endian.h>
#include <stdlib.h>
#include <stdio.h>

void *sqfs_read_table(int fd, sqfs_compressor_t *cmp, size_t table_size,
		      uint64_t location, uint64_t lower_limit,
		      uint64_t upper_limit)
{
	size_t diff, block_count, blk_idx = 0;
	uint64_t start, *locations;
	sqfs_meta_reader_t *m;
	void *data, *ptr;

	data = malloc(table_size);
	if (data == NULL) {
		perror("reading table");
		return NULL;
	}

	/* restore list from image */
	block_count = table_size / SQFS_META_BLOCK_SIZE;

	if ((table_size % SQFS_META_BLOCK_SIZE) != 0)
		++block_count;

	locations = alloc_array(sizeof(uint64_t), block_count);

	if (locations == NULL) {
		perror("allocation table location list");
		goto fail_data;
	}

	if (read_data_at("reading table locations", location,
			 fd, locations, sizeof(uint64_t) * block_count)) {
		goto fail_idx;
	}

	/* Read the actual data */
	m = sqfs_meta_reader_create(fd, cmp, lower_limit, upper_limit);
	if (m == NULL)
		goto fail_idx;

	ptr = data;

	while (table_size > 0) {
		start = le64toh(locations[blk_idx++]);

		if (sqfs_meta_reader_seek(m, start, 0))
			goto fail;

		diff = SQFS_META_BLOCK_SIZE;
		if (diff > table_size)
			diff = table_size;

		if (sqfs_meta_reader_read(m, ptr, diff))
			goto fail;

		ptr = (char *)ptr + diff;
		table_size -= diff;
	}

	sqfs_meta_reader_destroy(m);
	free(locations);
	return data;
fail:
	sqfs_meta_reader_destroy(m);
fail_idx:
	free(locations);
fail_data:
	free(data);
	return NULL;
}
