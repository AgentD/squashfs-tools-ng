/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"

#include "meta_writer.h"
#include "highlevel.h"
#include "util.h"

#include <endian.h>
#include <stdlib.h>
#include <stdio.h>

int sqfs_write_table(int outfd, sqfs_super_t *super, compressor_t *cmp,
		     const void *data, size_t table_size, uint64_t *start)
{
	size_t block_count, list_size, diff, blkidx = 0;
	uint64_t block, *locations;
	meta_writer_t *m;
	uint32_t offset;
	int ret = -1;

	block_count = table_size / SQFS_META_BLOCK_SIZE;
	if ((table_size % SQFS_META_BLOCK_SIZE) != 0)
		++block_count;

	list_size = sizeof(uint64_t) * block_count;
	locations = malloc(list_size);

	if (locations == NULL) {
		perror("writing table");
		return -1;
	}

	/* Write actual data */
	m = meta_writer_create(outfd, cmp, false);
	if (m == NULL)
		goto out_idx;

	while (table_size > 0) {
		meta_writer_get_position(m, &block, &offset);
		locations[blkidx++] = htole64(super->bytes_used + block);

		diff = SQFS_META_BLOCK_SIZE;
		if (diff > table_size)
			diff = table_size;

		if (meta_writer_append(m, data, diff))
			goto out;

		data = (const char *)data + diff;
		table_size -= diff;
	}

	if (meta_writer_flush(m))
		goto out;

	meta_writer_get_position(m, &block, &offset);
	super->bytes_used += block;

	/* write location list */
	*start = super->bytes_used;

	if (write_data("writing table locations", outfd, locations, list_size))
		goto out;

	super->bytes_used += list_size;

	/* cleanup */
	ret = 0;
out:
	meta_writer_destroy(m);
out_idx:
	free(locations);
	return ret;
}
