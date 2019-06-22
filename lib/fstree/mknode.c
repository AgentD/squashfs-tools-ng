/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "fstree.h"

#include <string.h>
#include <stdlib.h>

tree_node_t *fstree_mknode(fstree_t *fs, tree_node_t *parent, const char *name,
			   size_t name_len, const char *extra,
			   const struct stat *sb)
{
	size_t size = sizeof(tree_node_t);
	tree_node_t *n;
	char *ptr;

	switch (sb->st_mode & S_IFMT) {
	case S_IFLNK:
		size += strlen(extra) + 1;
		break;
	case S_IFDIR:
		size += sizeof(*n->data.dir);
		break;
	case S_IFREG:
		size += sizeof(*n->data.file);
		size += (sb->st_size / fs->block_size) * sizeof(uint32_t);
		if (extra != NULL)
			size += strlen(extra) + 1;
		break;
	}

	n = calloc(1, size + name_len + 1);
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

	switch (sb->st_mode & S_IFMT) {
	case S_IFDIR:
		n->data.dir = (dir_info_t *)n->payload;
		break;
	case S_IFREG:
		n->data.file = (file_info_t *)n->payload;
		n->data.file->size = sb->st_size;
		if (extra == NULL)
			break;

		ptr = (char *)n->data.file->blocksizes;
		ptr += (sb->st_size / fs->block_size) * sizeof(uint32_t);
		n->data.file->input_file = ptr;
		strcpy(n->data.file->input_file, extra);
		break;
	case S_IFLNK:
		n->mode = S_IFLNK | 0777;
		n->data.slink_target = (char *)n->payload;
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
}
