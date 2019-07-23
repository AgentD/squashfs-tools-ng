/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"

#include "meta_reader.h"
#include "highlevel.h"
#include "util.h"

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

int id_table_read(id_table_t *tbl, int fd, sqfs_super_t *super,
		  compressor_t *cmp)
{
	size_t i, block_count, count, diff;
	uint64_t blocks[32];
	meta_reader_t *m;
	uint32_t *ptr;

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

	tbl->ids = calloc(super->id_count, sizeof(uint32_t));
	if (tbl->ids == NULL) {
		perror("reading ID table");
		return -1;
	}

	tbl->num_ids = super->id_count;
	tbl->max_ids = super->id_count;

	if (lseek(fd, super->id_table_start, SEEK_SET) == (off_t)-1)
		goto fail_seek;

	block_count = super->id_count / 2048;
	if (super->id_count % 2048)
		++block_count;

	if (read_data("reading ID table", fd, blocks,
		      sizeof(blocks[0]) * block_count)) {
		return -1;
	}

	for (i = 0; i < block_count; ++i)
		blocks[i] = le64toh(blocks[i]);

	m = meta_reader_create(fd, cmp);
	if (m == NULL)
		return -1;

	count = super->id_count;
	ptr = tbl->ids;

	for (i = 0; i < block_count && count > 0; ++i) {
		diff = count < 2048 ? count : 2048;

		if (meta_reader_seek(m, blocks[i], 0))
			goto fail_meta;

		if (meta_reader_read(m, ptr, diff * sizeof(tbl->ids[0])))
			goto fail_meta;

		count -= diff;
		ptr += diff;
	}

	meta_reader_destroy(m);

	for (i = 0; i < tbl->num_ids; ++i)
		tbl->ids[i] = le32toh(tbl->ids[i]);

	return 0;
fail_meta:
	meta_reader_destroy(m);
	return -1;
fail_seek:
	perror("seeking to ID table");
	return -1;
}
