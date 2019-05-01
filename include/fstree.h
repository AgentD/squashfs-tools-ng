/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef FSTREE_H
#define FSTREE_H

#include <sys/types.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

typedef struct tree_node_t tree_node_t;
typedef struct file_info_t file_info_t;
typedef struct dir_info_t dir_info_t;
typedef struct fstree_t fstree_t;

struct file_info_t {
	char *input_file;
	file_info_t *frag_next;
	uint64_t size;
	uint64_t startblock;
	uint32_t fragment;
	uint32_t fragment_offset;
	uint32_t blocksizes[];
};

struct dir_info_t {
	tree_node_t *children;
	uint64_t size;
	uint64_t start_block;
	uint32_t block_offset;
	bool created_implicitly;
};

struct tree_node_t {
	tree_node_t *next;
	tree_node_t *parent;
	char *name;
	uint32_t uid;
	uint32_t gid;
	uint16_t mode;

	uint64_t inode_ref;
	uint32_t inode_num;
	int type;

	union {
		dir_info_t *dir;
		file_info_t *file;
		char *slink_target;
		uint64_t devno;
	} data;

	uint8_t payload[];
};

struct fstree_t {
	uint32_t default_uid;
	uint32_t default_gid;
	uint32_t default_mode;
	uint32_t default_mtime;
	size_t block_size;

	tree_node_t *root;
};

int fstree_init(fstree_t *fs, size_t block_size, uint32_t mtime,
		uint16_t default_mode, uint32_t default_uid,
		uint32_t default_gid);

void fstree_cleanup(fstree_t *fs);

tree_node_t *fstree_add(fstree_t *fs, const char *path, uint16_t mode,
			uint32_t uid, uint32_t gid, size_t extra_len);

tree_node_t *fstree_add_file(fstree_t *fs, const char *path, uint16_t mode,
			     uint32_t uid, uint32_t gid, uint64_t filesz,
			     const char *input);

int fstree_from_file(fstree_t *fs, const char *filename);

void fstree_sort(fstree_t *fs);

#endif /* FSTREE_H */
