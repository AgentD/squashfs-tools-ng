/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"

#include "meta_writer.h"
#include "highlevel.h"
#include "util.h"

#include <endian.h>
#include <stdio.h>

int sqfs_write_table(int outfd, sqfs_super_t *super, const void *data,
		     size_t entsize, size_t count, uint64_t *startblock,
		     compressor_t *cmp)
{
	size_t ent_per_blocks = SQFS_META_BLOCK_SIZE / entsize;
	uint64_t blocks[count / ent_per_blocks + 1], block;
	size_t i, blkidx = 0, tblsize;
	meta_writer_t *m;
	uint32_t offset;

	/* Write actual data. Whenever we cross a block boundary, remember
	   the block start offset */
	m = meta_writer_create(outfd, cmp, false);
	if (m == NULL)
		return -1;

	for (i = 0; i < count; ++i) {
		meta_writer_get_position(m, &block, &offset);

		if (blkidx == 0 || block > blocks[blkidx - 1])
			blocks[blkidx++] = block;

		if (meta_writer_append(m, data, entsize))
			goto fail;

		data = (const char *)data + entsize;
	}

	if (meta_writer_flush(m))
		goto fail;

	for (i = 0; i < blkidx; ++i)
		blocks[i] = htole64(blocks[i] + super->bytes_used);

	meta_writer_get_position(m, &block, &offset);
	super->bytes_used += block;
	meta_writer_destroy(m);

	/* write new index table */
	*startblock = super->bytes_used;
	tblsize = sizeof(blocks[0]) * blkidx;

	if (write_data("writing table index", outfd, blocks, tblsize))
		return -1;

	super->bytes_used += tblsize;
	return 0;
fail:
	meta_writer_destroy(m);
	return -1;
}
