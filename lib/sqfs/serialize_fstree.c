/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "meta_writer.h"
#include "highlevel.h"
#include "util.h"

#include <unistd.h>
#include <stdio.h>

int sqfs_serialize_fstree(int outfd, sqfs_super_t *super, fstree_t *fs,
			  compressor_t *cmp, id_table_t *idtbl)
{
	meta_writer_t *im, *dm;
	uint32_t offset;
	uint64_t block;
	int ret = -1;
	size_t i;

	im = meta_writer_create(outfd, cmp, false);
	if (im == NULL)
		return -1;

	dm = meta_writer_create(outfd, cmp, true);
	if (dm == NULL)
		goto out_im;

	for (i = 2; i < fs->inode_tbl_size; ++i) {
		if (meta_writer_write_inode(fs, idtbl, im, dm,
					    fs->inode_table[i])) {
			goto out;
		}
	}

	if (meta_writer_flush(im))
		goto out;

	if (meta_writer_flush(dm))
		goto out;

	super->root_inode_ref = fs->root->inode_ref;

	meta_writer_get_position(im, &block, &offset);
	super->inode_table_start = super->bytes_used;
	super->bytes_used += block;

	meta_writer_get_position(dm, &block, &offset);
	super->directory_table_start = super->bytes_used;
	super->bytes_used += block;

	if (meta_write_write_to_file(dm))
		goto out;

	ret = 0;
out:
	meta_writer_destroy(dm);
out_im:
	meta_writer_destroy(im);
	return ret;
}
