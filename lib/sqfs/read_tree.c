/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * read_tree.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "config.h"

#include "sqfs/meta_reader.h"
#include "sqfs/dir_reader.h"
#include "sqfs/compressor.h"
#include "sqfs/id_table.h"
#include "sqfs/super.h"
#include "sqfs/inode.h"
#include "sqfs/error.h"
#include "sqfs/dir.h"
#include "util.h"

#include <string.h>
#include <stdlib.h>

static int should_skip(int type, unsigned int flags)
{
	switch (type) {
	case SQFS_INODE_BDEV:
	case SQFS_INODE_CDEV:
	case SQFS_INODE_EXT_CDEV:
	case SQFS_INODE_EXT_BDEV:
		return (flags & SQFS_TREE_NO_DEVICES);
	case SQFS_INODE_SLINK:
	case SQFS_INODE_EXT_SLINK:
		return (flags & SQFS_TREE_NO_SLINKS);
	case SQFS_INODE_SOCKET:
	case SQFS_INODE_EXT_SOCKET:
		return(flags & SQFS_TREE_NO_SOCKETS);
	case SQFS_INODE_FIFO:
	case SQFS_INODE_EXT_FIFO:
		return (flags & SQFS_TREE_NO_FIFO);
	default:
		break;
	}

	return 0;
}

static bool would_be_own_parent(sqfs_tree_node_t *parent, sqfs_tree_node_t *n)
{
	sqfs_u32 inum = n->inode->base.inode_number;

	while (parent != NULL) {
		if (parent->inode->base.inode_number == inum)
			return true;

		parent = parent->parent;
	}

	return false;
}

static sqfs_tree_node_t *create_node(sqfs_inode_generic_t *inode,
				     const char *name)
{
	sqfs_tree_node_t *n;

	n = alloc_flex(sizeof(*n), 1, strlen(name) + 1);
	if (n == NULL)
		return NULL;

	n->inode = inode;
	strcpy((char *)n->name, name);
	return n;
}

static int fill_dir(sqfs_dir_reader_t *dr, sqfs_tree_node_t *root,
		    unsigned int flags)
{
	sqfs_tree_node_t *n, *prev, **tail;
	sqfs_inode_generic_t *inode;
	sqfs_dir_entry_t *ent;
	int err;

	tail = &root->children;

	for (;;) {
		err = sqfs_dir_reader_read(dr, &ent);
		if (err > 0)
			break;
		if (err < 0)
			return err;

		if (should_skip(ent->type, flags)) {
			free(ent);
			continue;
		}

		err = sqfs_dir_reader_get_inode(dr, &inode);
		if (err) {
			free(ent);
			return err;
		}

		n = create_node(inode, (const char *)ent->name);
		free(ent);

		if (n == NULL) {
			free(inode);
			return SQFS_ERROR_ALLOC;
		}

		if (would_be_own_parent(root, n)) {
			free(n);
			free(inode);
			return SQFS_ERROR_LINK_LOOP;
		}

		*tail = n;
		tail = &n->next;
		n->parent = root;
	}

	n = root->children;
	prev = NULL;

	while (n != NULL) {
		if (n->inode->base.type == SQFS_INODE_DIR ||
		    n->inode->base.type == SQFS_INODE_EXT_DIR) {
			if (!(flags & SQFS_TREE_NO_RECURSE)) {
				err = sqfs_dir_reader_open_dir(dr, n->inode, 0);
				if (err)
					return err;

				err = fill_dir(dr, n, flags);
				if (err)
					return err;
			}

			if (n->children == NULL &&
			    (flags & SQFS_TREE_NO_EMPTY)) {
				free(n->inode);
				if (prev == NULL) {
					root->children = root->children->next;
					free(n);
					n = root->children;
				} else {
					prev->next = n->next;
					free(n);
					n = prev->next;
				}
				continue;
			}
		}

		prev = n;
		n = n->next;
	}

	return 0;
}

static int resolve_ids(sqfs_tree_node_t *root, const sqfs_id_table_t *idtbl)
{
	sqfs_tree_node_t *it;
	int err;

	for (it = root->children; it != NULL; it = it->next)
		resolve_ids(it, idtbl);

	err = sqfs_id_table_index_to_id(idtbl, root->inode->base.uid_idx,
					&root->uid);
	if (err)
		return err;

	return sqfs_id_table_index_to_id(idtbl, root->inode->base.gid_idx,
					 &root->gid);
}

void sqfs_dir_tree_destroy(sqfs_tree_node_t *root)
{
	sqfs_tree_node_t *it;

	if (!root)
		return;

	while (root->children != NULL) {
		it = root->children;
		root->children = it->next;

		sqfs_dir_tree_destroy(it);
	}

	free(root->inode);
	free(root);
}

int sqfs_dir_reader_get_full_hierarchy(sqfs_dir_reader_t *rd,
				       const sqfs_id_table_t *idtbl,
				       const char *path, unsigned int flags,
				       sqfs_tree_node_t **out)
{
	sqfs_tree_node_t *root, *tail, *new;
	sqfs_inode_generic_t *inode;
	sqfs_dir_entry_t *ent;
	const char *ptr;
	int ret;

	if (flags & ~SQFS_TREE_ALL_FLAGS)
		return SQFS_ERROR_UNSUPPORTED;

	ret = sqfs_dir_reader_get_root_inode(rd, &inode);
	if (ret)
		return ret;

	root = tail = create_node(inode, "");
	if (root == NULL) {
		free(inode);
		return SQFS_ERROR_ALLOC;
	}
	inode = NULL;

	while (path != NULL && *path != '\0') {
		if (*path == '/') {
			while (*path == '/')
				++path;
			continue;
		}

		ret = sqfs_dir_reader_open_dir(rd, tail->inode, 0);
		if (ret)
			goto fail;

		ptr = strchr(path, '/');
		if (ptr == NULL) {

			if (ptr == NULL) {
				for (ptr = path; *ptr != '\0'; ++ptr)
					;
			}
		}

		for (;;) {
			ret = sqfs_dir_reader_read(rd, &ent);
			if (ret < 0)
				goto fail;
			if (ret > 0) {
				ret = SQFS_ERROR_NO_ENTRY;
				goto fail;
			}

			ret = strncmp((const char *)ent->name,
				      path, ptr - path);
			if (ret == 0 && ent->name[ptr - path] == '\0')
				break;
			free(ent);
		}

		ret = sqfs_dir_reader_get_inode(rd, &inode);
		if (ret) {
			free(ent);
			goto fail;
		}

		new = create_node(inode, (const char *)ent->name);
		free(ent);

		if (new == NULL) {
			free(inode);
			ret = SQFS_ERROR_ALLOC;
			goto fail;
		}

		inode = NULL;
		path = ptr;

		if (flags & SQFS_TREE_STORE_PARENTS) {
			tail->children = new;
			new->parent = tail;
			tail = new;
		} else {
			sqfs_dir_tree_destroy(root);
			root = tail = new;
		}
	}

	if (tail->inode->base.type == SQFS_INODE_DIR ||
	    tail->inode->base.type == SQFS_INODE_EXT_DIR) {
		ret = sqfs_dir_reader_open_dir(rd, tail->inode, 0);
		if (ret)
			goto fail;

		ret = fill_dir(rd, tail, flags);
		if (ret)
			goto fail;
	}

	ret = resolve_ids(root, idtbl);
	if (ret)
		goto fail;

	*out = root;
	return 0;
fail:
	sqfs_dir_tree_destroy(root);
	return ret;
}
