/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * fstree_from_file.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "fstree.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

#define STR(x) #x
#define STRVALUE(x) STR(x)

#define TEST_PATH STRVALUE(TESTPATH)

static FILE *open_read(const char *path)
{
	FILE *fp = fopen(path, "rb");

	if (fp == NULL) {
		perror(path);
		exit(EXIT_FAILURE);
	}

	return fp;
}

int main(void)
{
	tree_node_t *n;
	fstree_t fs;
	FILE *fp;

	fp = open_read(TEST_PATH);
	assert(fp != NULL);

	assert(fstree_init(&fs, NULL) == 0);
	assert(fstree_from_file(&fs, "testfile", fp) == 0);

	fstree_post_process(&fs);
	n = fs.root->data.dir.children;

	assert(n->mode == (S_IFBLK | 0600));
	assert(n->uid == 8);
	assert(n->gid == 9);
	assert(strcmp(n->name, "blkdev") == 0);
	assert(n->data.devno == makedev(42, 21));

	n = n->next;
	assert(n->mode == (S_IFCHR | 0600));
	assert(n->uid == 6);
	assert(n->gid == 7);
	assert(strcmp(n->name, "chardev") == 0);
	assert(n->data.devno == makedev(13, 37));

	n = n->next;
	assert(n->mode == (S_IFDIR | 0755));
	assert(n->uid == 4);
	assert(n->gid == 5);
	assert(strcmp(n->name, "dir") == 0);
	assert(n->data.dir.children == NULL);

	n = n->next;
	assert(n->mode == (S_IFDIR | 0755));
	assert(n->uid == 0);
	assert(n->gid == 0);
	assert(strcmp(n->name, "foo bar") == 0);
	assert(n->data.dir.children != NULL);

	assert(n->data.dir.children->next == NULL);
	assert(n->data.dir.children->mode == (S_IFDIR | 0755));
	assert(n->data.dir.children->uid == 0);
	assert(n->data.dir.children->gid == 0);
	assert(strcmp(n->data.dir.children->name, " test \"") == 0);
	assert(n->data.dir.children->data.dir.children == NULL);

	n = n->next;
	assert(n->mode == (S_IFIFO | 0644));
	assert(n->uid == 10);
	assert(n->gid == 11);
	assert(strcmp(n->name, "pipe") == 0);

	n = n->next;
	assert(n->mode == (S_IFLNK | 0777));
	assert(n->uid == 2);
	assert(n->gid == 3);
	assert(strcmp(n->name, "slink") == 0);
	fprintf(stderr, "'%s'\n", n->data.target);
	assert(strcmp(n->data.target, "slinktarget") == 0);

	n = n->next;
	assert(n->mode == (S_IFSOCK | 0555));
	assert(n->uid == 12);
	assert(n->gid == 13);
	assert(strcmp(n->name, "sock") == 0);
	assert(n->next == NULL);

	fclose(fp);
	fstree_cleanup(&fs);
	return EXIT_SUCCESS;
}
