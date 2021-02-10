/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * fill_files.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "rdsquashfs.h"

static struct file_ent {
	char *path;
	const sqfs_inode_generic_t *inode;
} *files = NULL;

static size_t num_files = 0, max_files = 0;
static size_t block_size = 0;

static int compare_files(const void *l, const void *r)
{
	sqfs_u32 lhs_frag_idx, lhs_frag_off, rhs_frag_idx, rhs_frag_off;
	sqfs_u64 lhs_size, rhs_size, lhs_start, rhs_start;
	const struct file_ent *lhs = l, *rhs = r;

	sqfs_inode_get_frag_location(lhs->inode, &lhs_frag_idx, &lhs_frag_off);
	sqfs_inode_get_file_block_start(lhs->inode, &lhs_start);
	sqfs_inode_get_file_size(lhs->inode, &lhs_size);

	sqfs_inode_get_frag_location(rhs->inode, &rhs_frag_idx, &rhs_frag_off);
	sqfs_inode_get_file_block_start(rhs->inode, &rhs_start);
	sqfs_inode_get_file_size(rhs->inode, &rhs_size);

	/* Files with fragments come first, ordered by ID.
	   In case of tie, files without data blocks come first,
	   and the others are ordered by start block. */
	if ((lhs_size % block_size) && (lhs_frag_off < block_size) &&
	    (lhs_frag_idx != 0xFFFFFFFF)) {
		if ((rhs_size % block_size) && (rhs_frag_off < block_size) &&
		    (rhs_frag_idx != 0xFFFFFFFF))
			return -1;

		if (lhs_frag_idx < rhs_frag_idx)
			return -1;

		if (lhs_frag_idx > rhs_frag_idx)
			return 1;

		if (lhs_size < block_size)
			return (rhs_size < block_size) ? 0 : -1;

		if (rhs_size < block_size)
			return 1;

		goto order_by_start;
	}

	if ((rhs_size % block_size) && (rhs_frag_off < block_size) &&
	    (rhs_frag_idx != 0xFFFFFFFF))
		return 1;

	/* order the rest by start block */
order_by_start:
	return lhs_start < rhs_start ? -1 : lhs_start > rhs_start ? 1 : 0;
}

static int add_file(const sqfs_tree_node_t *node)
{
	struct file_ent *new;
	size_t new_sz;
	char *path;

	if (num_files == max_files) {
		new_sz = max_files ? max_files * 2 : 256;
		new = realloc(files, sizeof(files[0]) * new_sz);

		if (new == NULL) {
			perror("expanding file list");
			return -1;
		}

		files = new;
		max_files = new_sz;
	}

	path = sqfs_tree_node_get_path(node);
	if (path == NULL) {
		perror("assembling file path");
		return -1;
	}

	if (canonicalize_name(path)) {
		fprintf(stderr, "Invalid file path '%s'\n", path);
		free(path);
		return -1;
	}

	files[num_files].path = path;
	files[num_files].inode = node->inode;
	num_files++;
	return 0;
}

static void clear_file_list(void)
{
	size_t i;

	for (i = 0; i < num_files; ++i)
		free(files[i].path);

	free(files);
	files = NULL;
	num_files = 0;
	max_files = 0;
}

static int gen_file_list_dfs(const sqfs_tree_node_t *n)
{
	if (!is_filename_sane((const char *)n->name, true)) {
		fprintf(stderr, "Found an entry named '%s', skipping.\n",
			n->name);
		return 0;
	}

	if (S_ISREG(n->inode->base.mode))
		return add_file(n);

	if (S_ISDIR(n->inode->base.mode)) {
		for (n = n->children; n != NULL; n = n->next) {
			if (gen_file_list_dfs(n))
				return -1;
		}
	}

	return 0;
}

static int fill_files(sqfs_data_reader_t *data, int flags)
{
	int ret, openflags;
	ostream_t *fp;
	size_t i;

	openflags = OSTREAM_OPEN_OVERWRITE;

	if (flags & UNPACK_NO_SPARSE)
		openflags |= OSTREAM_OPEN_SPARSE;

	for (i = 0; i < num_files; ++i) {
		fp = ostream_open_file(files[i].path, openflags);
		if (fp == NULL)
			return -1;

		if (!(flags & UNPACK_QUIET))
			printf("unpacking %s\n", files[i].path);

		ret = sqfs_data_reader_dump(files[i].path, data, files[i].inode,
					    fp, block_size);
		if (ret == 0)
			ret = ostream_flush(fp);

		sqfs_destroy(fp);
		if (ret)
			return -1;
	}

	return 0;
}

int fill_unpacked_files(size_t blk_sz, const sqfs_tree_node_t *root,
			sqfs_data_reader_t *data, int flags)
{
	int status;

	block_size = blk_sz;

	if (gen_file_list_dfs(root)) {
		clear_file_list();
		return -1;
	}

	qsort(files, num_files, sizeof(files[0]), compare_files);

	status = fill_files(data, flags);
	clear_file_list();
	return status;
}
