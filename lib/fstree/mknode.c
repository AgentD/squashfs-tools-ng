/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * mknode.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "fstree.h"

#include <string.h>
#include <stdlib.h>
#include <errno.h>

tree_node_t *fstree_mknode(tree_node_t *parent, const char *name,
			   size_t name_len, const char *extra,
			   const struct stat *sb)
{
	tree_node_t *n;
	size_t size;
	char *ptr;

	if (S_ISLNK(sb->st_mode) && extra == NULL) {
		errno = EINVAL;
		return NULL;
	}

	size = sizeof(tree_node_t) + name_len + 1;
	if (extra != NULL)
		size += strlen(extra) + 1;

	n = calloc(1, size);
	if (n == NULL)
		return NULL;

	if (parent != NULL) {
		n->next = parent->data.dir.children;
		parent->data.dir.children = n;
		n->parent = parent;
	}

	n->xattr_idx = 0xFFFFFFFF;
	n->uid = sb->st_uid;
	n->gid = sb->st_gid;
	n->mode = sb->st_mode;
	n->mod_time = sb->st_mtime;
	n->link_count = 1;
	n->name = (char *)n->payload;
	memcpy(n->name, name, name_len);

	if (extra != NULL) {
		ptr = n->name + name_len + 1;
		strcpy(ptr, extra);
	} else {
		ptr = NULL;
	}

	switch (sb->st_mode & S_IFMT) {
	case S_IFREG:
		n->data.file.input_file = ptr;
		break;
	case S_IFLNK:
		n->mode = S_IFLNK | 0777;
		n->data.target = ptr;
		break;
	case S_IFBLK:
	case S_IFCHR:
		n->data.devno = sb->st_rdev;
		break;
	case S_IFDIR:
		n->link_count = 2;
		break;
	default:
		break;
	}

	if (parent != NULL) {
		if (parent->link_count == 0x0FFFF) {
			free(n);
			errno = EMLINK;
			return NULL;
		}

		parent->link_count++;
	}

	return n;
}
