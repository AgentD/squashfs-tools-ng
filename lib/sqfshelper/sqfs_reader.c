/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * sqfs_reader.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "highlevel.h"

#include <string.h>
#include <unistd.h>
#include <fcntl.h>

int sqfs_reader_open(sqfs_reader_t *rd, const char *filename, int rdtree_flags)
{
	memset(rd, 0, sizeof(*rd));

	rd->sqfsfd = open(filename, O_RDONLY);
	if (rd->sqfsfd < 0) {
		perror(filename);
		return -1;
	}

	if (sqfs_super_read(&rd->super, rd->sqfsfd))
		goto fail_fd;

	if (!compressor_exists(rd->super.compression_id)) {
		fprintf(stderr, "%s: unknown compressor used.\n", filename);
		goto fail_fd;
	}

	rd->cmp = compressor_create(rd->super.compression_id, false,
				    rd->super.block_size, NULL);
	if (rd->cmp == NULL)
		goto fail_fd;

	if (rd->super.flags & SQFS_FLAG_COMPRESSOR_OPTIONS) {
		if (rd->cmp->read_options(rd->cmp, rd->sqfsfd))
			goto fail_cmp;
	}

	if (rd->super.flags & SQFS_FLAG_NO_XATTRS)
		rdtree_flags &= ~RDTREE_READ_XATTR;

	if (deserialize_fstree(&rd->fs, &rd->super, rd->cmp, rd->sqfsfd,
			       rdtree_flags)) {
		goto fail_cmp;
	}

	fstree_gen_file_list(&rd->fs);

	rd->data = data_reader_create(rd->sqfsfd, &rd->super, rd->cmp);
	if (rd->data == NULL)
		goto fail_fs;

	return 0;
fail_fs:
	fstree_cleanup(&rd->fs);
fail_cmp:
	rd->cmp->destroy(rd->cmp);
fail_fd:
	close(rd->sqfsfd);
	memset(rd, 0, sizeof(*rd));
	return -1;
}

void sqfs_reader_close(sqfs_reader_t *rd)
{
	data_reader_destroy(rd->data);
	fstree_cleanup(&rd->fs);
	rd->cmp->destroy(rd->cmp);
	close(rd->sqfsfd);
	memset(rd, 0, sizeof(*rd));
}
