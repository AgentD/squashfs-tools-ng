/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "mkfs.h"
#include "util.h"

int sqfs_write_inodes(sqfs_info_t *info)
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

	im = meta_writer_create(info->outfd, info->cmp);
	if (im == NULL)
		goto fail_tmp;

	dm = meta_writer_create(tmpfd, info->cmp);
	if (dm == NULL)
		goto fail_im;

	for (i = 2; i < info->fs.inode_tbl_size; ++i) {
		if (write_inode(&info->fs, &info->idtbl, im, dm,
				info->fs.inode_table[i])) {
			goto fail;
		}
	}

	if (meta_writer_flush(im))
		goto fail;

	if (meta_writer_flush(dm))
		goto fail;

	info->super.root_inode_ref = info->fs.root->inode_ref;

	meta_writer_get_position(im, &block, &offset);
	info->super.inode_table_start = info->super.bytes_used;
	info->super.bytes_used += block;

	info->super.directory_table_start = info->super.bytes_used;
	meta_writer_get_position(dm, &block, &offset);
	info->super.bytes_used += block;

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
		ret = write_retry(info->outfd, buffer, diff);

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
