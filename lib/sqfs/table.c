/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "meta_writer.h"
#include "table.h"
#include "util.h"

#include <endian.h>
#include <stdio.h>

static int sqfs_write_table(int outfd, sqfs_super_t *super, const void *data,
			    size_t entsize, size_t count, uint64_t *startblock,
			    compressor_t *cmp)
{
	size_t ent_per_blocks = SQFS_META_BLOCK_SIZE / entsize;
	uint64_t blocks[count / ent_per_blocks + 1];
	size_t i, blkidx = 0, tblsize;
	meta_writer_t *m;
	ssize_t ret;

	/* Write actual data. Whenever we cross a block boundary, remember
	   the block start offset */
	m = meta_writer_create(outfd, cmp);
	if (m == NULL)
		return -1;

	for (i = 0; i < count; ++i) {
		if (blkidx == 0 || m->block_offset > blocks[blkidx - 1])
			blocks[blkidx++] = m->block_offset;

		if (meta_writer_append(m, data, entsize))
			goto fail;

		data = (const char *)data + entsize;
	}

	if (meta_writer_flush(m))
		goto fail;

	for (i = 0; i < blkidx; ++i)
		blocks[i] = htole64(blocks[i] + super->bytes_used);

	super->bytes_used += m->block_offset;
	meta_writer_destroy(m);

	/* write new index table */
	*startblock = super->bytes_used;
	tblsize = sizeof(blocks[0]) * blkidx;

	ret = write_retry(outfd, blocks, tblsize);
	if (ret < 0) {
		perror("writing index table");
		return -1;
	}

	if ((size_t)ret < tblsize) {
		fputs("index table truncated\n", stderr);
		return -1;
	}

	super->bytes_used += tblsize;
	return 0;
fail:
	meta_writer_destroy(m);
	return -1;
}

int sqfs_write_fragment_table(int outfd, sqfs_super_t *super,
			      sqfs_fragment_t *fragments, size_t count,
			      compressor_t *cmp)
{
	super->fragment_entry_count = count;

	return sqfs_write_table(outfd, super, fragments, sizeof(fragments[0]),
				count, &super->fragment_table_start, cmp);
}

int sqfs_write_ids(int outfd, sqfs_super_t *super, uint32_t *id_tbl,
		   size_t count, compressor_t *cmp)
{
	size_t i;
	int ret;

	for (i = 0; i < count; ++i)
		id_tbl[i] = htole32(id_tbl[i]);

	super->id_count = count;

	ret = sqfs_write_table(outfd, super, id_tbl, sizeof(id_tbl[0]),
			       count, &super->id_table_start, cmp);

	for (i = 0; i < count; ++i)
		id_tbl[i] = htole32(id_tbl[i]);

	return ret;
}
