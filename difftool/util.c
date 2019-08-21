/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * util.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "sqfsdiff.h"

char *node_path(tree_node_t *n)
{
	char *path = fstree_get_path(n);

	if (path == NULL) {
		perror("get path");
		return NULL;
	}

	if (canonicalize_name(path)) {
		fputs("[BUG] canonicalization of fstree_get_path failed!!\n",
		      stderr);
		free(path);
		return NULL;
	}

	return path;
}
