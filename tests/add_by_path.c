/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * add_by_path.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "fstree.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

int main(void)
{
	tree_node_t *a, *b;
	struct stat sb;
	fstree_t fs;
	char *opts;

	opts = strdup("mode=0755,uid=21,gid=42");
	assert(fstree_init(&fs, opts) == 0);
	free(opts);

	memset(&sb, 0, sizeof(sb));
	sb.st_mode = S_IFDIR | 0750;
	sb.st_uid = 1000;
	sb.st_gid = 100;

	a = fstree_add_generic(&fs, "dir", &sb, NULL);
	assert(a != NULL);
	assert(strcmp(a->name, "dir") == 0);
	assert(a->mode == sb.st_mode);
	assert(a->uid == sb.st_uid);
	assert(a->gid == sb.st_gid);
	assert(a->parent == fs.root);
	assert(a->next == NULL);
	assert(fs.root->data.dir.children == a);
	assert(!a->data.dir.created_implicitly);

	memset(&sb, 0, sizeof(sb));
	sb.st_mode = S_IFBLK | 0640;
	sb.st_rdev = 1234;

	b = fstree_add_generic(&fs, "blkdev", &sb, NULL);
	assert(b != NULL);
	assert(b != a);
	assert(strcmp(b->name, "blkdev") == 0);
	assert(b->mode == sb.st_mode);
	assert(b->uid == sb.st_uid);
	assert(b->gid == sb.st_gid);
	assert(b->parent == fs.root);
	assert(b->data.devno == sb.st_rdev);
	assert(b->next == a);
	assert(fs.root->data.dir.children == b);

	assert(fstree_add_generic(&fs, "blkdev/foo", &sb, NULL) == NULL);
	assert(errno == ENOTDIR);

	assert(fstree_add_generic(&fs, "dir", &sb, NULL) == NULL);
	assert(errno == EEXIST);

	memset(&sb, 0, sizeof(sb));
	sb.st_mode = S_IFDIR | 0755;
	assert(fstree_add_generic(&fs, "dir", &sb, NULL) == NULL);
	assert(errno == EEXIST);

	memset(&sb, 0, sizeof(sb));
	sb.st_mode = S_IFCHR | 0444;
	b = fstree_add_generic(&fs, "dir/chrdev", &sb, NULL);
	assert(b != NULL);
	assert(b->mode == sb.st_mode);
	assert(b->uid == sb.st_uid);
	assert(b->gid == sb.st_gid);
	assert(b->parent == a);
	assert(b->data.devno == sb.st_rdev);
	assert(b->next == NULL);
	assert(a->data.dir.children == b);

	b = fstree_add_generic(&fs, "dir/foo/chrdev", &sb, NULL);
	assert(b != NULL);
	assert(b->next == NULL);
	assert(b->mode == sb.st_mode);
	assert(b->uid == sb.st_uid);
	assert(b->gid == sb.st_gid);
	assert(b->parent != a);
	assert(b->parent->parent == a);
	assert(b->data.devno == sb.st_rdev);
	assert(b->next == NULL);
	assert(a->data.dir.children != b);

	b = b->parent;
	assert(b->data.dir.created_implicitly);
	assert(b->mode == (S_IFDIR | 0755));
	assert(b->uid == 21);
	assert(b->gid == 42);

	memset(&sb, 0, sizeof(sb));
	sb.st_mode = S_IFDIR | 0750;
	sb.st_uid = 1000;
	sb.st_gid = 100;

	a = fstree_add_generic(&fs, "dir/foo", &sb, NULL);
	assert(a != NULL);
	assert(a == b);
	assert(!a->data.dir.created_implicitly);
	assert(a->mode == sb.st_mode);
	assert(a->uid == sb.st_uid);
	assert(a->gid == sb.st_gid);

	assert(fstree_add_generic(&fs, "dir/foo", &sb, NULL) == NULL);
	assert(errno == EEXIST);

	fstree_cleanup(&fs);
	return EXIT_SUCCESS;
}
