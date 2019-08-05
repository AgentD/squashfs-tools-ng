/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * node_compare.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "difftool.h"

int node_compare(tree_node_t *a, tree_node_t *b)
{
	char *path = node_path(a);
	tree_node_t *ait, *bit;
	int ret, status = 0;

	if (path == NULL)
		return -1;

	if ((a->mode & S_IFMT) != (b->mode & S_IFMT)) {
		fprintf(stdout, "%s has a different type\n", path);
		free(path);
		return 1;
	}

	if (!(compare_flags & COMPARE_NO_PERM)) {
		if ((a->mode & ~S_IFMT) != (b->mode & ~S_IFMT)) {
			fprintf(stdout, "%s has different permissions\n",
				path);
			status = 1;
		}
	}

	if (!(compare_flags & COMPARE_NO_OWNER)) {
		if (a->uid != b->uid || a->gid != b->gid) {
			fprintf(stdout, "%s has different ownership\n", path);
			status = 1;
		}
	}

	switch (a->mode & S_IFMT) {
	case S_IFSOCK:
	case S_IFIFO:
		break;
	case S_IFBLK:
	case S_IFCHR:
		if (a->data.devno != b->data.devno) {
			fprintf(stdout, "%s has different device number\n",
				path);
			status = 1;
		}
		break;
	case S_IFLNK:
		if (strcmp(a->data.slink_target, b->data.slink_target) != 0) {
			fprintf(stdout, "%s has a different link target\n",
				path);
		}
		break;
	case S_IFDIR:
		ret = compare_dir_entries(a, b);
		if (ret < 0) {
			status = -1;
			break;
		}
		if (ret > 0)
			status = 1;

		free(path);
		path = NULL;

		ait = a->data.dir->children;
		bit = b->data.dir->children;

		while (ait != NULL && bit != NULL) {
			ret = node_compare(ait, bit);
			if (ret < 0)
				return -1;
			if (ret > 0)
				status = 1;

			ait = ait->next;
			bit = bit->next;
		}
		break;
	case S_IFREG:
		ret = compare_files(a->data.file, b->data.file, path);
		if (ret < 0) {
			status = -1;
		} else if (ret > 0) {
			fprintf(stdout, "regular file %s differs\n", path);
			status = 1;
		}
		break;
	default:
		fprintf(stdout, "%s has unknown type, ignoring\n", path);
		break;
	}

	free(path);
	return status;
}
