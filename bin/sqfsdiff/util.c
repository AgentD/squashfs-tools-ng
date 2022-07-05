/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * util.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "sqfsdiff.h"

char *node_path(const sqfs_tree_node_t *n)
{
	char *path;
	int ret;

	ret = sqfs_tree_node_get_path(n, &path);
	if (ret != 0) {
		sqfs_perror(NULL, "get path", ret);
		return NULL;
	}

	if (canonicalize_name(path)) {
		fprintf(stderr, "failed to canonicalization '%s'\n", path);
		sqfs_free(path);
		return NULL;
	}

	return path;
}
