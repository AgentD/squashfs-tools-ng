/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "fstree.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

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

int fstree_init(fstree_t *fs, size_t block_size, uint32_t mtime,
		uint16_t default_mode, uint32_t default_uid,
		uint32_t default_gid)
{
	memset(fs, 0, sizeof(*fs));

	fs->defaults.st_uid = default_uid;
	fs->defaults.st_gid = default_gid;
	fs->defaults.st_mode = S_IFDIR | (default_mode & 07777);
	fs->defaults.st_mtime = mtime;
	fs->defaults.st_ctime = mtime;
	fs->defaults.st_atime = mtime;
	fs->defaults.st_blksize = block_size;
	fs->block_size = block_size;

	if (str_table_init(&fs->xattr_keys, FSTREE_XATTR_KEY_BUCKETS))
		return -1;

	if (str_table_init(&fs->xattr_values, FSTREE_XATTR_VALUE_BUCKETS)) {
		str_table_cleanup(&fs->xattr_keys);
		return -1;
	}

	fs->root = fstree_mknode(fs, NULL, "", 0, NULL, &fs->defaults);

	if (fs->root == NULL) {
		perror("initializing file system tree");
		str_table_cleanup(&fs->xattr_values);
		str_table_cleanup(&fs->xattr_keys);
		return -1;
	}

	return 0;
}

void fstree_cleanup(fstree_t *fs)
{
	tree_xattr_t *xattr;

	while (fs->xattr != NULL) {
		xattr = fs->xattr;
		fs->xattr = xattr->next;
		free(xattr);
	}

	str_table_cleanup(&fs->xattr_keys);
	str_table_cleanup(&fs->xattr_values);
	free_recursive(fs->root);
	free(fs->inode_table);
	memset(fs, 0, sizeof(*fs));
}
