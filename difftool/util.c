/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * util.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "sqfsdiff.h"

char *node_path(const sqfs_tree_node_t *n)
{
	char *path = sqfs_tree_node_get_path(n);

	if (path == NULL) {
		perror("get path");
		return NULL;
	}

	if (canonicalize_name(path)) {
		fprintf(stderr, "failed to canonicalization '%s'\n", path);
		free(path);
		return NULL;
	}

	return path;
}
