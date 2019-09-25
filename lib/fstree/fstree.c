/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * fstree.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "fstree.h"
#include "util.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

enum {
	DEF_UID = 0,
	DEF_GID,
	DEF_MODE,
	DEF_MTIME,
};

static const char *defaults[] = {
	[DEF_UID] = "uid",
	[DEF_GID] = "gid",
	[DEF_MODE] = "mode",
	[DEF_MTIME] = "mtime",
	NULL
};

static int process_defaults(struct stat *sb, char *subopts)
{
	char *value;
	long lval;
	int i;

	while (*subopts != '\0') {
		i = getsubopt(&subopts, (char *const *)defaults, &value);

		if (value == NULL) {
			fprintf(stderr, "Missing value for option %s\n",
				defaults[i]);
			return -1;
		}

		switch (i) {
		case DEF_UID:
			lval = strtol(value, NULL, 0);
			if (lval < 0)
				goto fail_uv;
			if (lval > (long)INT32_MAX)
				goto fail_ov;
			sb->st_uid = lval;
			break;
		case DEF_GID:
			lval = strtol(value, NULL, 0);
			if (lval < 0)
				goto fail_uv;
			if (lval > (long)INT32_MAX)
				goto fail_ov;
			sb->st_gid = lval;
			break;
		case DEF_MODE:
			lval = strtol(value, NULL, 0);
			if (lval < 0)
				goto fail_uv;
			if (lval > 07777)
				goto fail_ov;
			sb->st_mode = S_IFDIR | (uint16_t)lval;
			break;
		case DEF_MTIME:
			lval = strtol(value, NULL, 0);
			if (lval < 0)
				goto fail_uv;
			if (lval > (long)INT32_MAX)
				goto fail_ov;
			sb->st_mtime = lval;
			break;
		default:
			fprintf(stderr, "Unknown option '%s'\n", value);
			return -1;
		}
	}
	return 0;
fail_uv:
	fprintf(stderr, "%s: value must be positive\n", defaults[i]);
	return -1;
fail_ov:
	fprintf(stderr, "%s: value too large\n", defaults[i]);
	return -1;
}

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

int fstree_init(fstree_t *fs, size_t block_size, char *defaults)
{
	memset(fs, 0, sizeof(*fs));
	fs->defaults.st_mode = S_IFDIR | 0755;
	fs->defaults.st_blksize = block_size;
	fs->defaults.st_mtime = get_source_date_epoch();
	fs->block_size = block_size;

	if (defaults != NULL && process_defaults(&fs->defaults, defaults) != 0)
		return -1;

	if (str_table_init(&fs->xattr_keys, FSTREE_XATTR_KEY_BUCKETS))
		return -1;

	if (str_table_init(&fs->xattr_values, FSTREE_XATTR_VALUE_BUCKETS)) {
		str_table_cleanup(&fs->xattr_keys);
		return -1;
	}

	fs->root = fstree_mknode(NULL, "", 0, NULL, &fs->defaults);

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
