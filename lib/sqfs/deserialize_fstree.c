/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"

#include "meta_reader.h"
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

static int fill_dir(meta_reader_t *ir, meta_reader_t *dr, tree_node_t *root,
		    sqfs_super_t *super, id_table_t *idtbl, int flags)
{
	sqfs_inode_generic_t *inode;
	sqfs_dir_header_t hdr;
	sqfs_dir_entry_t *ent;
	tree_node_t *n, *prev;
	uint64_t block_start;
	size_t size, diff;
	uint32_t i;

	size = root->data.dir->size;
	if (size <= sizeof(hdr))
		return 0;

	block_start = root->data.dir->start_block;
	block_start += super->directory_table_start;

	if (meta_reader_seek(dr, block_start, root->data.dir->block_offset))
		return -1;

	while (size > sizeof(hdr)) {
		if (meta_reader_read_dir_header(dr, &hdr))
			return -1;

		size -= sizeof(hdr);

		for (i = 0; i <= hdr.count && size > sizeof(*ent); ++i) {
			ent = meta_reader_read_dir_ent(dr);
			if (ent == NULL)
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

			inode = meta_reader_read_inode(ir, super,
						       hdr.start_block,
						       ent->offset);
			if (inode == NULL) {
				free(ent);
				return -1;
			}

			n = tree_node_from_inode(inode, idtbl,
						 (char *)ent->name,
						 super->block_size);
			free(ent);
			free(inode);

			if (n == NULL)
				return -1;

			n->parent = root;
			n->next = root->data.dir->children;
			root->data.dir->children = n;
		}
	}

	n = root->data.dir->children;
	prev = NULL;

	while (n != NULL) {
		if (S_ISDIR(n->mode)) {
			if (fill_dir(ir, dr, n, super, idtbl, flags))
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

int deserialize_fstree(fstree_t *out, sqfs_super_t *super, compressor_t *cmp,
		       int fd, int flags)
{
	sqfs_inode_generic_t *root;
	meta_reader_t *ir, *dr;
	uint64_t block_start;
	id_table_t idtbl;
	int status = -1;
	size_t offset;

	ir = meta_reader_create(fd, cmp);
	if (ir == NULL)
		return -1;

	dr = meta_reader_create(fd, cmp);
	if (dr == NULL)
		goto out_ir;

	if (id_table_init(&idtbl))
		goto out_dr;

	if (id_table_read(&idtbl, fd, super, cmp))
		goto out_id;

	block_start = super->root_inode_ref >> 16;
	offset = super->root_inode_ref & 0xFFFF;
	root = meta_reader_read_inode(ir, super, block_start, offset);
	if (root == NULL)
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

	out->root = tree_node_from_inode(root, &idtbl, "", super->block_size);
	free(root);
	root = NULL;

	if (out->root == NULL)
		goto out_id;

	if (fill_dir(ir, dr, out->root, super, &idtbl, flags))
		goto fail_fs;

	tree_node_sort_recursive(out->root);

	status = 0;
out_id:
	id_table_cleanup(&idtbl);
out_dr:
	meta_reader_destroy(dr);
out_ir:
	meta_reader_destroy(ir);
	return status;
fail_fs:
	fstree_cleanup(out);
	goto out_id;
}
