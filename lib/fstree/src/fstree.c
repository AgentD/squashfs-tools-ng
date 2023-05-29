/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * fstree.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "util/util.h"
#include "fstree.h"

#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>

static sqfs_u32 clamp_timestamp(sqfs_s64 ts)
{
	if (ts < 0)
		return 0;
	if (ts > 0x0FFFFFFFFLL)
		return 0xFFFFFFFF;
	return ts;
}

static void free_recursive(tree_node_t *n)
{
	tree_node_t *it;

	if (S_ISDIR(n->mode)) {
		while (n->data.children != NULL) {
			it = n->data.children;
			n->data.children = it->next;

			free_recursive(it);
		}
	}

	free(n);
}

static tree_node_t *child_by_name(tree_node_t *root, const char *name,
				  size_t len)
{
	tree_node_t *n = root->data.children;

	while (n != NULL) {
		if (strncmp(n->name, name, len) == 0 && n->name[len] == '\0')
			break;

		n = n->next;
	}

	return n;
}

static void insert_sorted(tree_node_t *root, tree_node_t *n)
{
	tree_node_t *it = root->data.children, *prev = NULL;

	while (it != NULL && strcmp(it->name, n->name) < 0) {
		prev = it;
		it = it->next;
	}

	n->parent = root;
	n->next = it;

	if (prev == NULL) {
		root->data.children = n;
	} else {
		prev->next = n;
	}
}

static tree_node_t *mknode(fstree_t *fs, tree_node_t *parent, const char *name,
			   size_t name_len, const char *extra,
			   const dir_entry_t *ent)
{
	tree_node_t *n;
	size_t size;
	char *ptr;

	size = sizeof(tree_node_t) + name_len + 1;
	if (extra != NULL)
		size += strlen(extra) + 1;

	n = calloc(1, size);
	if (n == NULL)
		return NULL;

	n->xattr_idx = 0xFFFFFFFF;
	n->uid = ent->uid;
	n->gid = ent->gid;
	n->mode = ent->mode;
	n->mod_time = clamp_timestamp(ent->mtime);
	n->link_count = 1;
	n->name = (char *)n->payload;
	memcpy(n->name, name, name_len);

	if (extra != NULL) {
		ptr = n->name + name_len + 1;
		strcpy(ptr, extra);

		if (ent->flags & DIR_ENTRY_FLAG_HARD_LINK) {
			if (canonicalize_name(ptr)) {
				free(n);
				errno = EINVAL;
				return NULL;
			}
		}
	} else {
		ptr = NULL;
	}

	if (ent->flags & DIR_ENTRY_FLAG_HARD_LINK) {
		n->mode = S_IFLNK | 0777;
		n->flags |= FLAG_LINK_IS_HARD;
	}

	switch (n->mode & S_IFMT) {
	case S_IFREG:
		n->data.file.input_file = ptr;
		break;
	case S_IFLNK:
		n->mode = S_IFLNK | 0777;
		n->data.target = ptr;
		break;
	case S_IFBLK:
	case S_IFCHR:
		n->data.devno = ent->rdev;
		break;
	case S_IFDIR:
		n->link_count = 2;
		break;
	default:
		break;
	}

	if (parent->link_count == 0xFFFFFFFF) {
		free(n);
		errno = EMLINK;
		return NULL;
	}

	if (ent->flags & DIR_ENTRY_FLAG_HARD_LINK) {
		n->next_by_type = fs->links_unresolved;
		fs->links_unresolved = n;
	}

	insert_sorted(parent, n);
	parent->link_count++;
	return n;
}

int fstree_init(fstree_t *fs, const fstree_defaults_t *defaults)
{
	memset(fs, 0, sizeof(*fs));
	fs->defaults = *defaults;

	fs->root = calloc(1, sizeof(tree_node_t) + 1);
	if (fs->root == NULL) {
		perror("initializing file system tree");
		return -1;
	}

	fs->root->xattr_idx = 0xFFFFFFFF;
	fs->root->uid = defaults->uid;
	fs->root->gid = defaults->gid;
	fs->root->mode = S_IFDIR | (defaults->mode & 07777);
	fs->root->mod_time = defaults->mtime;
	fs->root->link_count = 2;
	fs->root->name = (char *)fs->root->payload;
	fs->root->flags |= FLAG_DIR_CREATED_IMPLICITLY;
	return 0;
}

void fstree_cleanup(fstree_t *fs)
{
	free_recursive(fs->root);
	free(fs->inodes);
	memset(fs, 0, sizeof(*fs));
}

tree_node_t *fstree_get_node_by_path(fstree_t *fs, tree_node_t *root,
				     const char *path, bool create_implicitly,
				     bool stop_at_parent)
{
	const char *end;
	tree_node_t *n;
	size_t len;

	while (*path != '\0') {
		while (*path == '/')
			++path;

		if (!S_ISDIR(root->mode)) {
			errno = ENOTDIR;
			return NULL;
		}

		end = strchr(path, '/');
		if (end == NULL) {
			if (stop_at_parent)
				break;

			len = strlen(path);
		} else {
			len = end - path;
		}

		n = child_by_name(root, path, len);

		if (n == NULL) {
			dir_entry_t ent;

			if (!create_implicitly) {
				errno = ENOENT;
				return NULL;
			}

			memset(&ent, 0, sizeof(ent));
			ent.mode = S_IFDIR | (fs->defaults.mode & 07777);
			ent.uid = fs->defaults.uid;
			ent.gid = fs->defaults.gid;
			ent.mtime = fs->defaults.mtime;

			n = mknode(fs, root, path, len, NULL, &ent);
			if (n == NULL)
				return NULL;

			n->flags |= FLAG_DIR_CREATED_IMPLICITLY;
		}

		root = n;
		path = path + len;
	}

	return root;
}

tree_node_t *fstree_add_generic(fstree_t *fs, const dir_entry_t *ent,
				const char *extra)
{
	tree_node_t *child, *parent;
	const char *name;

	if (S_ISLNK(ent->mode) && extra == NULL) {
		errno = EINVAL;
		return NULL;
	}

	if (ent->name[0] == '\0') {
		child = fs->root;
		assert(child != NULL);
		goto out;
	}

	parent = fstree_get_node_by_path(fs, fs->root, ent->name, true, true);
	if (parent == NULL)
		return NULL;

	name = strrchr(ent->name, '/');
	name = (name == NULL ? ent->name : (name + 1));

	child = child_by_name(parent, name, strlen(name));
out:
	if (child != NULL) {
		if (!S_ISDIR(child->mode) || !S_ISDIR(ent->mode) ||
		    !(child->flags & FLAG_DIR_CREATED_IMPLICITLY)) {
			errno = EEXIST;
			return NULL;
		}

		child->uid = ent->uid;
		child->gid = ent->gid;
		child->mode = ent->mode;
		child->mod_time = ent->mtime;
		child->flags &= ~FLAG_DIR_CREATED_IMPLICITLY;
		return child;
	}

	return mknode(fs, parent, name, strlen(name), extra, ent);
}
