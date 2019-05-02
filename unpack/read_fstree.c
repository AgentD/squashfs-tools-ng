/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "unsquashfs.h"


static int fill_dir(meta_reader_t *ir, meta_reader_t *dr, tree_node_t *root,
		    sqfs_super_t *super, id_table_t *idtbl)
{
	sqfs_inode_generic_t *inode;
	sqfs_dir_header_t hdr;
	sqfs_dir_entry_t *ent;
	uint64_t block_start;
	size_t size, diff;
	tree_node_t *n;
	uint32_t i;

	block_start = root->data.dir->start_block;
	block_start += super->directory_table_start;

	if (meta_reader_seek(dr, block_start, root->data.dir->block_offset))
		return -1;

	size = root->data.dir->size;

	while (size != 0) {
		if (meta_reader_read_dir_header(dr, &hdr))
			return -1;

		size -= sizeof(hdr) > size ? size : sizeof(hdr);

		for (i = 0; i <= hdr.count; ++i) {
			ent = meta_reader_read_dir_ent(dr);
			if (ent == NULL)
				return -1;

			diff = sizeof(*ent) + strlen((char *)ent->name);
			size -= diff > size ? size : diff;

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

	for (n = root->data.dir->children; n != NULL; n = n->next) {
		if (S_ISDIR(n->mode)) {
			if (fill_dir(ir, dr, n, super, idtbl))
				return -1;
		}
	}

	return 0;
}

int read_fstree(fstree_t *out, int fd, sqfs_super_t *super, compressor_t *cmp)
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
	out->default_uid = 0;
	out->default_gid = 0;
	out->default_mode = 0755;
	out->default_mtime = super->modification_time;

	out->root = tree_node_from_inode(root, &idtbl, "", super->block_size);
	free(root);
	root = NULL;

	if (out->root == NULL)
		goto out_id;

	if (fill_dir(ir, dr, out->root, super, &idtbl))
		goto fail_fs;

	fstree_sort(out);

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
