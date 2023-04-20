/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * stats.c
 *
 * Copyright (C) 2023 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "fstree.h"
#include <string.h>

static void count_dfs(const tree_node_t *n, fstree_stats_t *out)
{
	switch (n->mode & S_IFMT) {
	case S_IFSOCK:
	case S_IFIFO:
		out->num_ipc += 1;
		break;
	case S_IFLNK:
		if (n->flags & FLAG_LINK_IS_HARD) {
			out->num_links += 1;
		} else {
			out->num_slinks += 1;
		}
		break;
	case S_IFREG:
		out->num_files += 1;
		break;
	case S_IFBLK:
	case S_IFCHR:
		out->num_devices += 1;
		break;
	case S_IFDIR:
		out->num_dirs += 1;
		for (n = n->data.children; n != NULL; n = n->next) {
			count_dfs(n, out);
		}
		break;
	default:
		break;
	}
}

void fstree_collect_stats(const fstree_t *fs, fstree_stats_t *out)
{
	memset(out, 0, sizeof(*out));

	count_dfs(fs->root, out);
}
