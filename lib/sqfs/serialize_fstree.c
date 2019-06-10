/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "meta_writer.h"
#include "table.h"
#include "util.h"

#include <unistd.h>
#include <stdio.h>

int sqfs_serialize_fstree(int outfd, sqfs_super_t *super, fstree_t *fs,
			  compressor_t *cmp, id_table_t *idtbl)
{
	meta_writer_t *im, *dm;
	uint8_t buffer[1024];
	uint32_t offset;
	uint64_t block;
	size_t i, diff;
	ssize_t ret;
	FILE *tmp;
	int tmpfd;

	tmp = tmpfile();
	if (tmp == NULL) {
		perror("tmpfile");
		return -1;
	}

	tmpfd = fileno(tmp);

	im = meta_writer_create(outfd, cmp);
	if (im == NULL)
		goto fail_tmp;

	dm = meta_writer_create(tmpfd, cmp);
	if (dm == NULL)
		goto fail_im;

	for (i = 2; i < fs->inode_tbl_size; ++i) {
		if (meta_writer_write_inode(fs, idtbl, im, dm,
					    fs->inode_table[i])) {
			goto fail;
		}
	}

	if (meta_writer_flush(im))
		goto fail;

	if (meta_writer_flush(dm))
		goto fail;

	super->root_inode_ref = fs->root->inode_ref;

	meta_writer_get_position(im, &block, &offset);
	super->inode_table_start = super->bytes_used;
	super->bytes_used += block;

	super->directory_table_start = super->bytes_used;
	meta_writer_get_position(dm, &block, &offset);
	super->bytes_used += block;

	if (lseek(tmpfd, 0, SEEK_SET) == (off_t)-1) {
		perror("rewind on directory temp file");
		goto fail;
	}

	for (;;) {
		ret = read_retry(tmpfd, buffer, sizeof(buffer));

		if (ret < 0) {
			perror("read from temp file");
			goto fail;
		}
		if (ret == 0)
			break;

		diff = ret;
		ret = write_retry(outfd, buffer, diff);

		if (ret < 0) {
			perror("write to image file");
			goto fail;
		}
		if ((size_t)ret < diff) {
			fputs("copying meta data to image file: "
			      "truncated write\n", stderr);
			goto fail;
		}
	}

	meta_writer_destroy(dm);
	meta_writer_destroy(im);
	fclose(tmp);
	return 0;
fail:
	meta_writer_destroy(dm);
fail_im:
	meta_writer_destroy(im);
fail_tmp:
	fclose(tmp);
	return -1;
}
