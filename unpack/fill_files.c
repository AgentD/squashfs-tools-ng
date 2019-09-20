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

static uint32_t get_frag_idx(const sqfs_inode_generic_t *inode)
{
	if (inode->base.type == SQFS_INODE_EXT_FILE)
		return inode->data.file_ext.fragment_idx;

	return inode->data.file.fragment_index;
}

static uint32_t get_frag_off(const sqfs_inode_generic_t *inode)
{
	if (inode->base.type == SQFS_INODE_EXT_FILE)
		return inode->data.file_ext.fragment_offset;

	return inode->data.file.fragment_offset;
}

static uint64_t get_size(const sqfs_inode_generic_t *inode)
{
	if (inode->base.type == SQFS_INODE_EXT_FILE)
		return inode->data.file_ext.file_size;

	return inode->data.file.file_size;
}

static uint64_t get_start(const sqfs_inode_generic_t *inode)
{
	if (inode->base.type == SQFS_INODE_EXT_FILE)
		return inode->data.file_ext.blocks_start;

	return inode->data.file.blocks_start;
}

static bool has_fragment(const struct file_ent *ent)
{
	if (get_size(ent->inode) % block_size == 0)
		return false;

	return get_frag_off(ent->inode) < block_size &&
		(get_frag_idx(ent->inode) != 0xFFFFFFFF);
}

static int compare_files(const void *l, const void *r)
{
	const struct file_ent *lhs = l, *rhs = r;

	/* Files with fragments come first, ordered by ID.
	   In case of tie, files without data blocks come first,
	   and the others are ordered by start block. */
	if (has_fragment(lhs)) {
		if (!(has_fragment(rhs)))
			return -1;

		if (get_frag_idx(lhs->inode) < get_frag_idx(rhs->inode))
			return -1;

		if (get_frag_idx(lhs->inode) > get_frag_idx(rhs->inode))
			return 1;

		if (get_size(lhs->inode) < block_size)
			return (get_size(rhs->inode) < block_size) ? 0 : -1;

		if (get_size(rhs->inode) < block_size)
			return 1;

		goto order_by_start;
	}

	if (has_fragment(rhs))
		return 1;

	/* order the rest by start block */
order_by_start:
	return get_start(lhs->inode) < get_start(rhs->inode) ? -1 :
		get_start(lhs->inode) > get_start(rhs->inode) ? 1 : 0;
}

static int add_file(const sqfs_tree_node_t *node)
{
	size_t new_sz;
	char *path;
	void *new;

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
	size_t i;
	int fd;

	for (i = 0; i < num_files; ++i) {
		fd = open(files[i].path, O_WRONLY);
		if (fd < 0) {
			fprintf(stderr, "unpacking %s: %s\n",
				files[i].path, strerror(errno));
			return -1;
		}

		if (!(flags & UNPACK_QUIET))
			printf("unpacking %s\n", files[i].path);

		if (sqfs_data_reader_dump(data, files[i].inode, fd, block_size,
					  (flags & UNPACK_NO_SPARSE) == 0)) {
			close(fd);
			return -1;
		}

		close(fd);
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
