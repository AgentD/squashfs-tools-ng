/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * mknode.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "fstree.h"
#include "util.h"

#include <string.h>
#include <stdlib.h>
#include <errno.h>

tree_node_t *fstree_mknode(fstree_t *fs, tree_node_t *parent, const char *name,
			   size_t name_len, const char *extra,
			   const struct stat *sb)
{
	size_t size = sizeof(tree_node_t), block_count = 0, total;
	tree_node_t *n;
	char *ptr;

	switch (sb->st_mode & S_IFMT) {
	case S_IFLNK:
		if (extra == NULL) {
			errno = EINVAL;
			return NULL;
		}
		if (SZ_ADD_OV(size, strlen(extra), &size) ||
		    SZ_ADD_OV(size, 1, &size)) {
			goto fail_ov;
		}
		break;
	case S_IFDIR:
		if (SZ_ADD_OV(size, sizeof(*n->data.dir), &size))
			goto fail_ov;
		break;
	case S_IFREG:
		block_count = (sb->st_size / fs->block_size);
		if ((sb->st_size % fs->block_size) != 0)
			++block_count;

		if (SZ_MUL_OV(block_count, sizeof(n->data.file->block_size[0]),
			      &total)) {
			goto fail_ov;
		}

		if (SZ_ADD_OV(size, sizeof(*n->data.file), &size) ||
		    SZ_ADD_OV(size, total, &size)) {
			goto fail_ov;
		}

		if (extra != NULL) {
			if (SZ_ADD_OV(size, strlen(extra), &size) ||
			    SZ_ADD_OV(size, 1, &size)) {
				goto fail_ov;
			}
		}
		break;
	}

	if (SZ_ADD_OV(size, name_len, &total) ||
	    SZ_ADD_OV(total, 1, &total)) {
		goto fail_ov;
	}

	n = calloc(1, total);
	if (n == NULL)
		return NULL;

	if (parent != NULL) {
		n->next = parent->data.dir->children;
		parent->data.dir->children = n;
		n->parent = parent;
	}

	n->uid = sb->st_uid;
	n->gid = sb->st_gid;
	n->mode = sb->st_mode;
	n->mod_time = sb->st_mtime;

	switch (sb->st_mode & S_IFMT) {
	case S_IFDIR:
		n->data.dir = (dir_info_t *)n->payload;
		break;
	case S_IFREG:
		n->data.file = (file_info_t *)n->payload;
		n->data.file->size = sb->st_size;
		n->data.file->fragment = 0xFFFFFFFF;
		n->data.file->fragment_offset = 0xFFFFFFFF;
		if (extra == NULL)
			break;

		ptr = (char *)n->data.file->block_size;
		ptr += block_count * sizeof(n->data.file->block_size[0]);
		n->data.file->input_file = ptr;
		strcpy(n->data.file->input_file, extra);
		break;
	case S_IFLNK:
		n->mode = S_IFLNK | 0777;
		n->data.slink_target = (char *)n->payload;
		if (extra != NULL)
			strcpy(n->data.slink_target, extra);
		break;
	case S_IFBLK:
	case S_IFCHR:
		n->data.devno = sb->st_rdev;
		break;
	}

	n->name = (char *)n + size;
	memcpy(n->name, name, name_len);
	return n;
fail_ov:
	errno = EOVERFLOW;
	return NULL;
}
