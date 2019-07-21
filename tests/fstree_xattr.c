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

	assert(fstree_init(&fs, 512, NULL) == 0);

	memset(&sb, 0, sizeof(sb));
	sb.st_mode = S_IFCHR | 0640;
	sb.st_rdev = 1337;

	a = fstree_add_generic(&fs, "/a", &sb, NULL);
	b = fstree_add_generic(&fs, "/b", &sb, NULL);
	c = fstree_add_generic(&fs, "/c", &sb, NULL);
	d = fstree_add_generic(&fs, "/d", &sb, NULL);
	assert(a != NULL);
	assert(b != NULL);
	assert(c != NULL);
	assert(d != NULL);

	assert(fstree_add_xattr(&fs, a, "foo", "bar") == 0);

	assert(fstree_add_xattr(&fs, b, "foo", "bar") == 0);
	assert(fstree_add_xattr(&fs, b, "baz", "qux") == 0);

	assert(fstree_add_xattr(&fs, c, "foo", "something else") == 0);

	assert(fstree_add_xattr(&fs, d, "baz", "qux") == 0);
	assert(fstree_add_xattr(&fs, d, "foo", "bar") == 0);

	assert(a->xattr != NULL);
	assert(b->xattr != NULL);
	assert(c->xattr != NULL);
	assert(d->xattr != NULL);

	assert(a->xattr != b->xattr);
	assert(a->xattr != c->xattr);
	assert(a->xattr != d->xattr);
	assert(b->xattr != c->xattr);
	assert(b->xattr != d->xattr);
	assert(c->xattr != d->xattr);

	assert(a->xattr->num_attr == 1);
	assert(b->xattr->num_attr == 2);
	assert(c->xattr->num_attr == 1);
	assert(d->xattr->num_attr == 2);

	assert(a->xattr->attr[0].key_index == b->xattr->attr[0].key_index);
	assert(a->xattr->attr[0].value_index == b->xattr->attr[0].value_index);

	assert(a->xattr->attr[0].key_index == d->xattr->attr[1].key_index);
	assert(a->xattr->attr[0].value_index == d->xattr->attr[1].value_index);

	assert(a->xattr->attr[0].key_index == c->xattr->attr[0].key_index);
	assert(a->xattr->attr[0].value_index != c->xattr->attr[0].value_index);

	assert(b->xattr->attr[1].key_index == d->xattr->attr[0].key_index);
	assert(b->xattr->attr[1].value_index == d->xattr->attr[0].value_index);

	fstree_xattr_deduplicate(&fs);

	assert(b->xattr == d->xattr);
	assert(a->xattr != b->xattr);
	assert(a->xattr != c->xattr);

	fstree_cleanup(&fs);
	return EXIT_SUCCESS;
}
