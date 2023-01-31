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
	sqfs_drop(sqfs->xwr);
	sqfs_drop(sqfs->dirwr);
	sqfs_drop(sqfs->dm);
	sqfs_drop(sqfs->im);
	sqfs_drop(sqfs->idtbl);
	sqfs_drop(sqfs->data);
	sqfs_drop(sqfs->blkwr);
	sqfs_drop(sqfs->fragtbl);
	sqfs_drop(sqfs->cmp);
	sqfs_drop(sqfs->uncmp);
	fstree_cleanup(&sqfs->fs);
	sqfs_drop(sqfs->outfile);

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

