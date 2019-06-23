/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "fstree.h"

#include <sys/sysmacros.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

static const char *testdesc =
"# comment line\n"
"slink /slink 0644 2 3 slinktarget\n"
"dir /dir 0755 4 5\n"
"nod /chardev 0600 6 7 c 13:37\n"
"nod /blkdev 0600 8 9 b 42:21\n"
"pipe /pipe 0644 10 11\n"
"  sock  /sock  0555  12  13  ";

int main(void)
{
	tree_node_t *n;
	fstree_t fs;
	char *ptr;
	FILE *fp;

	ptr = strdup(testdesc);
	assert(ptr != NULL);

	fp = fmemopen(ptr, strlen(ptr), "r");
	assert(fp != NULL);

	assert(fstree_init(&fs, 512, NULL) == 0);
	assert(fstree_from_file(&fs, "testfile", fp) == 0);

	tree_node_sort_recursive(fs.root);
	n = fs.root->data.dir->children;

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
	assert(n->data.dir->children == NULL);

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
	assert(strcmp(n->data.slink_target, "slinktarget") == 0);

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
