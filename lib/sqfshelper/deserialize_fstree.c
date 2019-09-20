/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * deserialize_fstree.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "sqfs/meta_reader.h"
#include "sqfs/dir.h"

#include "highlevel.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static bool node_would_be_own_parent(tree_node_t *root, tree_node_t *n)
{
	while (root != NULL) {
		if (root->inode_num == n->inode_num)
			return true;

		root = root->parent;
	}

	return false;
}

static bool is_name_sane(const char *name)
{
	if (strchr(name, '/') != NULL || strchr(name, '\\') != NULL)
		goto fail;

	if (strcmp(name, "..") == 0 || strcmp(name, ".") == 0)
		goto fail;

	return true;
fail:
	fprintf(stderr, "WARNING: Found directory entry named '%s', "
		"skipping\n", name);
	return false;
}

static int fill_dir(sqfs_dir_reader_t *dr,
		    tree_node_t *root, sqfs_id_table_t *idtbl,
		    fstree_t *fs)
{
	sqfs_inode_generic_t *inode;
	sqfs_dir_entry_t *ent;
	tree_node_t *n;
	int err;

	for (;;) {
		err = sqfs_dir_reader_read(dr, &ent);
		if (err > 0)
			break;
		if (err < 0)
			return -1;

		if (!is_name_sane((const char *)ent->name)) {
			free(ent);
			continue;
		}

		err = sqfs_dir_reader_get_inode(dr, &inode);
		if (err) {
			free(ent);
			return err;
		}

		n = tree_node_from_inode(inode, idtbl, (char *)ent->name);
		if (n == NULL) {
			free(ent);
			free(inode);
			return -1;
		}

		if (node_would_be_own_parent(root, n)) {
			fputs("WARNING: Found a directory that "
			      "contains itself, skipping loop back "
			      "reference!\n", stderr);
			free(n);
			free(ent);
			free(inode);
			continue;
		}

		free(ent);

		n->inode = inode;
		n->parent = root;
		n->next = root->data.dir->children;
		root->data.dir->children = n;
	}

	for (n = root->data.dir->children; n != NULL; n = n->next) {
		if (S_ISDIR(n->mode)) {
			err = sqfs_dir_reader_open_dir(dr, n->inode);
			if (err)
				return -1;

			if (fill_dir(dr, n, idtbl, fs))
				return -1;
		}

		free(n->inode);
		n->inode = NULL;
	}

	return 0;
}

int deserialize_fstree(fstree_t *out, sqfs_super_t *super,
		       sqfs_compressor_t *cmp, sqfs_file_t *file)
{
	sqfs_inode_generic_t *root;
	sqfs_id_table_t *idtbl;
	sqfs_dir_reader_t *dr;
	int status = -1;

	dr = sqfs_dir_reader_create(super, cmp, file);
	if (dr == NULL)
		return -1;

	idtbl = sqfs_id_table_create();
	if (idtbl == NULL)
		goto out_dr;

	if (sqfs_id_table_read(idtbl, file, super, cmp))
		goto out_id;

	if (sqfs_dir_reader_get_root_inode(dr, &root))
		goto out_id;

	if (root->base.type != SQFS_INODE_DIR &&
	    root->base.type != SQFS_INODE_EXT_DIR) {
		free(root);
		fputs("File system root inode is not a directory inode!\n",
		      stderr);
		goto out_id;
	}

	memset(out, 0, sizeof(*out));
	out->block_size = super->block_size;
	out->defaults.st_uid = 0;
	out->defaults.st_gid = 0;
	out->defaults.st_mode = 0755;
	out->defaults.st_mtime = super->modification_time;

	out->root = tree_node_from_inode(root, idtbl, "");

	if (out->root == NULL) {
		free(root);
		goto out_id;
	}

	if (sqfs_dir_reader_open_dir(dr, root)) {
		free(root);
		goto fail_fs;
	}

	free(root);

	if (fill_dir(dr, out->root, idtbl, out))
		goto fail_fs;

	tree_node_sort_recursive(out->root);

	status = 0;
out_id:
	sqfs_id_table_destroy(idtbl);
out_dr:
	sqfs_dir_reader_destroy(dr);
	return status;
fail_fs:
	fstree_cleanup(out);
	goto out_id;
}
