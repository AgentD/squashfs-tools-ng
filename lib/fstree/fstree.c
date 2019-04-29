/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "fstree.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

static tree_node_t *mknode(tree_node_t *parent, const char *name,
			   size_t name_len, size_t extra_len,
			   uint16_t mode, uint32_t uid, uint32_t gid)
{
	size_t size = sizeof(tree_node_t) + extra_len;
	tree_node_t *n;

	switch (mode & S_IFMT) {
	case S_IFDIR:
		size += sizeof(*n->data.dir);
		break;
	case S_IFREG:
		size += sizeof(*n->data.file);
		break;
	}

	n = calloc(1, size + name_len + 1);
	if (n == NULL)
		return NULL;

	if (parent != NULL) {
		n->next = parent->data.dir->children;
		parent->data.dir->children = n;
	}

	n->uid = uid;
	n->gid = gid;
	n->mode = mode;

	switch (mode & S_IFMT) {
	case S_IFDIR:
		n->data.dir = (dir_info_t *)n->payload;
		break;
	case S_IFREG:
		n->data.file = (file_info_t *)n->payload;
		break;
	case S_IFLNK:
		n->data.slink_target = (char *)n->payload;
		break;
	}

	n->name = (char *)n + size;
	memcpy(n->name, name, name_len);
	return n;
}

static void free_recursive(tree_node_t *n)
{
	tree_node_t *it;

	if (S_ISDIR(n->mode)) {
		while (n->data.dir->children != NULL) {
			it = n->data.dir->children;
			n->data.dir->children = it->next;

			free_recursive(it);
		}
	}

	free(n);
}

static tree_node_t *child_by_name(tree_node_t *root, const char *name,
				  size_t len)
{
	tree_node_t *n = root->data.dir->children;

	while (n != NULL) {
		if (strncmp(n->name, name, len) == 0 && n->name[len] == '\0')
			break;

		n = n->next;
	}

	return n;
}

static tree_node_t *get_parent_node(fstree_t *fs, tree_node_t *root,
				    const char *path)
{
	const char *end;
	tree_node_t *n;

	for (;;) {
		if (!S_ISDIR(root->mode)) {
			errno = ENOTDIR;
			return NULL;
		}

		end = strchr(path, '/');
		if (end == NULL)
			break;

		n = child_by_name(root, path, end - path);

		if (n == NULL) {
			n = mknode(root, path, end - path, 0,
				   S_IFDIR | fs->default_mode,
				   fs->default_uid, fs->default_gid);
			if (n == NULL)
				return NULL;

			n->data.dir->created_implicitly = true;
		}

		root = n;
		path = end + 1;
	}

	return root;
}

tree_node_t *fstree_add(fstree_t *fs, const char *path, uint16_t mode,
			uint32_t uid, uint32_t gid, size_t extra_len)
{
	tree_node_t *child, *parent;
	const char *name;

	name = strrchr(path, '/');
	name = (name == NULL ? path : (name + 1));

	parent = get_parent_node(fs, fs->root, path);
	if (parent == NULL)
		return NULL;

	child = child_by_name(parent, name, strlen(name));
	if (child != NULL) {
		if (S_ISDIR(child->mode) && S_ISDIR(mode) &&
		    child->data.dir->created_implicitly) {
			child->data.dir->created_implicitly = false;
			return child;
		}

		errno = EEXIST;
		return NULL;
	}

	return mknode(parent, name, strlen(name), extra_len, mode, uid, gid);
}

tree_node_t *fstree_add_file(fstree_t *fs, const char *path, uint16_t mode,
			     uint32_t uid, uint32_t gid, uint64_t filesz,
			     const char *input)
{
	tree_node_t *node;
	size_t count, extra;
	char *ptr;

	count = filesz / fs->block_size;
	extra = sizeof(uint32_t) * count + strlen(input) + 1;

	mode &= 07777;
	node = fstree_add(fs, path, S_IFREG | mode, uid, gid, extra);

	if (node != NULL) {
		ptr = (char *)(node->data.file->blocksizes + count);
		strcpy(ptr, input);

		node->data.file->input_file = ptr;
		node->data.file->size = filesz;
	}
	return node;
}

int fstree_init(fstree_t *fs, size_t block_size, uint32_t mtime,
		uint16_t default_mode, uint32_t default_uid,
		uint32_t default_gid)
{
	memset(fs, 0, sizeof(*fs));

	fs->default_uid = default_uid;
	fs->default_gid = default_gid;
	fs->default_mode = default_mode & 07777;
	fs->default_mtime = mtime;
	fs->block_size = block_size;

	fs->root = mknode(NULL, "", 0, 0, S_IFDIR | fs->default_mode,
			  default_uid, default_gid);

	if (fs->root == NULL) {
		perror("initializing file system tree");
		return -1;
	}

	return 0;
}

void fstree_cleanup(fstree_t *fs)
{
	free_recursive(fs->root);
	memset(fs, 0, sizeof(*fs));
}
