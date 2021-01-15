/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * cleanup.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "simple_writer.h"

#include <stdlib.h>

void sqfs_writer_cleanup(sqfs_writer_t *sqfs, int status)
{
	if (sqfs->xwr != NULL)
		sqfs_destroy(sqfs->xwr);

	sqfs_destroy(sqfs->dirwr);
	sqfs_destroy(sqfs->dm);
	sqfs_destroy(sqfs->im);
	sqfs_destroy(sqfs->idtbl);
	sqfs_destroy(sqfs->data);
	sqfs_destroy(sqfs->blkwr);
	sqfs_destroy(sqfs->fragtbl);
	sqfs_destroy(sqfs->cmp);
	sqfs_destroy(sqfs->uncmp);
	fstree_cleanup(&sqfs->fs);
	sqfs_destroy(sqfs->outfile);

	if (status != EXIT_SUCCESS) {
#if defined(_WIN32) || defined(__WINDOWS__)
		WCHAR *path = path_to_windows(sqfs->filename);

		if (path != NULL)
			DeleteFileW(path);

		free(path);
#else
		unlink(sqfs->filename);
#endif
	}
}

