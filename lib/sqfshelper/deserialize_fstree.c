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

static int should_skip(int type, int flags)
{
	switch (type) {
	case SQFS_INODE_BDEV:
	case SQFS_INODE_CDEV:
	case SQFS_INODE_EXT_CDEV:
	case SQFS_INODE_EXT_BDEV:
		return (flags & RDTREE_NO_DEVICES);
	case SQFS_INODE_SLINK:
	case SQFS_INODE_EXT_SLINK:
		return (flags & RDTREE_NO_SLINKS);
	case SQFS_INODE_SOCKET:
	case SQFS_INODE_EXT_SOCKET:
		return(flags & RDTREE_NO_SOCKETS);
	case SQFS_INODE_FIFO:
	case SQFS_INODE_EXT_FIFO:
		return (flags & RDTREE_NO_FIFO);
	}
	return 0;
}

static int restore_xattr(sqfs_xattr_reader_t *xr, fstree_t *fs,
			 tree_node_t *node, sqfs_inode_generic_t *inode)
{
	uint32_t idx;

	switch (inode->base.type) {
	case SQFS_INODE_EXT_DIR:
		idx = inode->data.dir_ext.xattr_idx;
		break;
	case SQFS_INODE_EXT_FILE:
		idx = inode->data.file_ext.xattr_idx;
		break;
	case SQFS_INODE_EXT_SLINK:
		idx = inode->data.slink_ext.xattr_idx;
		break;
	case SQFS_INODE_EXT_BDEV:
	case SQFS_INODE_EXT_CDEV:
		idx = inode->data.dev_ext.xattr_idx;
		break;
	case SQFS_INODE_EXT_FIFO:
	case SQFS_INODE_EXT_SOCKET:
		idx = inode->data.ipc_ext.xattr_idx;
		break;
	default:
		return 0;
	}

	return xattr_reader_restore_node(xr, fs, node, idx);
}

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

static int fill_dir(sqfs_meta_reader_t *ir, sqfs_meta_reader_t *dr,
		    tree_node_t *root, sqfs_super_t *super,
		    sqfs_id_table_t *idtbl,
		    fstree_t *fs, sqfs_xattr_reader_t *xr, int flags)
{
	sqfs_inode_generic_t *inode;
	sqfs_dir_header_t hdr;
	sqfs_dir_entry_t *ent;
	tree_node_t *n, *prev;
	uint64_t block_start;
	size_t size, diff;
	uint32_t i;
	int err;

	size = root->data.dir->size;
	if (size <= sizeof(hdr))
		return 0;

	block_start = root->data.dir->start_block;
	block_start += super->directory_table_start;

	if (sqfs_meta_reader_seek(dr, block_start,
				  root->data.dir->block_offset)) {
		return -1;
	}

	while (size > sizeof(hdr)) {
		if (sqfs_meta_reader_read_dir_header(dr, &hdr))
			return -1;

		size -= sizeof(hdr);

		for (i = 0; i <= hdr.count && size > sizeof(*ent); ++i) {
			if (sqfs_meta_reader_read_dir_ent(dr, &ent))
				return -1;

			diff = sizeof(*ent) + strlen((char *)ent->name);
			if (diff > size) {
				free(ent);
				break;
			}
			size -= diff;

			if (should_skip(ent->type, flags)) {
				free(ent);
				continue;
			}

			if (!is_name_sane((const char *)ent->name)) {
				free(ent);
				continue;
			}

			err = sqfs_meta_reader_read_inode(ir, super,
							  hdr.start_block,
							  ent->offset, &inode);
			if (err) {
				free(ent);
				return err;
			}

			n = tree_node_from_inode(inode, idtbl,
						 (char *)ent->name,
						 fs->block_size);

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

			if (flags & RDTREE_READ_XATTR) {
				if (restore_xattr(xr, fs, n, inode)) {
					free(n);
					free(ent);
					free(inode);
					return -1;
				}
			}

			free(ent);
			free(inode);

			n->parent = root;
			n->next = root->data.dir->children;
			root->data.dir->children = n;
		}
	}

	n = root->data.dir->children;
	prev = NULL;

	while (n != NULL) {
		if (S_ISDIR(n->mode)) {
			if (fill_dir(ir, dr, n, super, idtbl, fs, xr, flags))
				return -1;

			if (n->data.dir->children == NULL &&
			    (flags & RDTREE_NO_EMPTY)) {
				if (prev == NULL) {
					root->data.dir->children = n->next;
					free(n);
					n = root->data.dir->children;
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

int deserialize_fstree(fstree_t *out, sqfs_super_t *super,
		       sqfs_compressor_t *cmp, sqfs_file_t *file, int flags)
{
	uint64_t block_start, limit;
	sqfs_meta_reader_t *ir, *dr;
	sqfs_inode_generic_t *root;
	sqfs_xattr_reader_t *xr;
	sqfs_id_table_t *idtbl;
	int status = -1;
	size_t offset;

	ir = sqfs_meta_reader_create(file, cmp, super->inode_table_start,
				     super->directory_table_start);
	if (ir == NULL)
		return -1;

	limit = super->id_table_start;
	if (super->export_table_start < limit)
		limit = super->export_table_start;
	if (super->fragment_table_start < limit)
		limit = super->fragment_table_start;

	dr = sqfs_meta_reader_create(file, cmp, super->directory_table_start,
				     limit);
	if (dr == NULL)
		goto out_ir;

	idtbl = sqfs_id_table_create();
	if (idtbl == NULL)
		goto out_dr;

	if (sqfs_id_table_read(idtbl, file, super, cmp))
		goto out_id;

	xr = sqfs_xattr_reader_create(file, super, cmp);
	if (xr == NULL)
		goto out_id;

	if (sqfs_xattr_reader_load_locations(xr))
		goto out_xr;

	block_start = super->root_inode_ref >> 16;
	offset = super->root_inode_ref & 0xFFFF;
	if (sqfs_meta_reader_read_inode(ir, super, block_start, offset, &root))
		goto out_xr;

	if (root->base.type != SQFS_INODE_DIR &&
	    root->base.type != SQFS_INODE_EXT_DIR) {
		free(root);
		fputs("File system root inode is not a directory inode!\n",
		      stderr);
		goto out_xr;
	}

	memset(out, 0, sizeof(*out));
	out->block_size = super->block_size;
	out->defaults.st_uid = 0;
	out->defaults.st_gid = 0;
	out->defaults.st_mode = 0755;
	out->defaults.st_mtime = super->modification_time;

	out->root = tree_node_from_inode(root, idtbl, "", out->block_size);

	if (out->root == NULL) {
		free(root);
		goto out_xr;
	}

	if (flags & RDTREE_READ_XATTR) {
		if (str_table_init(&out->xattr_keys,
				   FSTREE_XATTR_KEY_BUCKETS)) {
			free(root);
			goto fail_fs;
		}

		if (str_table_init(&out->xattr_values,
				   FSTREE_XATTR_VALUE_BUCKETS)) {
			free(root);
			goto fail_fs;
		}

		if (restore_xattr(xr, out, out->root, root)) {
			free(root);
			goto fail_fs;
		}
	}

	free(root);

	if (fill_dir(ir, dr, out->root, super, idtbl, out, xr, flags))
		goto fail_fs;

	tree_node_sort_recursive(out->root);

	status = 0;
out_xr:
	sqfs_xattr_reader_destroy(xr);
out_id:
	sqfs_id_table_destroy(idtbl);
out_dr:
	sqfs_meta_reader_destroy(dr);
out_ir:
	sqfs_meta_reader_destroy(ir);
	return status;
fail_fs:
	fstree_cleanup(out);
	goto out_xr;
}
