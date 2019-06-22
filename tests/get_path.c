/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "fstree.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

int main(void)
{
	tree_node_t *a, *b, *c, *d;
	struct stat sb;
	fstree_t fs;
	char *str;

	assert(fstree_init(&fs, 512, 1337, 0755, 21, 42) == 0);

	memset(&sb, 0, sizeof(sb));
	sb.st_mode = S_IFDIR | 0750;
	sb.st_uid = 1000;
	sb.st_gid = 100;

	a = fstree_add_generic(&fs, "foo", &sb, NULL);
	b = fstree_add_generic(&fs, "foo/bar", &sb, NULL);
	c = fstree_add_generic(&fs, "foo/bar/baz", &sb, NULL);
	d = fstree_add_generic(&fs, "foo/bar/baz/dir", &sb, NULL);

	str = fstree_get_path(fs.root);
	assert(str != NULL);
	assert(strcmp(str, "/") == 0);
	free(str);

	str = fstree_get_path(a);
	assert(str != NULL);
	assert(strcmp(str, "/foo") == 0);
	free(str);

	str = fstree_get_path(b);
	assert(str != NULL);
	assert(strcmp(str, "/foo/bar") == 0);
	free(str);

	str = fstree_get_path(c);
	assert(str != NULL);
	assert(strcmp(str, "/foo/bar/baz") == 0);
	free(str);

	str = fstree_get_path(d);
	assert(str != NULL);
	assert(strcmp(str, "/foo/bar/baz/dir") == 0);
	free(str);

	fstree_cleanup(&fs);
	return EXIT_SUCCESS;
}
