/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * fstree_sort.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "fstree.h"
#include "common.h"
#include "util/test.h"

int main(int argc, char **argv)
{
	tree_node_t *a, *b, *c, *d;
	fstree_defaults_t fsd;
	struct stat sb;
	fstree_t fs;
	int ret;
	(void)argc; (void)argv;

	memset(&sb, 0, sizeof(sb));
	sb.st_mode = S_IFBLK | 0600;
	sb.st_rdev = 1337;

	/* in order */
	TEST_ASSERT(parse_fstree_defaults(&fsd, NULL) == 0);
	ret = fstree_init(&fs, &fsd);
	TEST_EQUAL_I(ret, 0);

	a = fstree_mknode(fs.root, "a", 1, NULL, &sb);
	TEST_NOT_NULL(a);
	TEST_ASSERT(fs.root->data.children == a);
	TEST_NULL(a->next);

	b = fstree_mknode(fs.root, "b", 1, NULL, &sb);
	TEST_NOT_NULL(a);
	TEST_ASSERT(fs.root->data.children == a);
	TEST_ASSERT(a->next == b);
	TEST_NULL(b->next);

	c = fstree_mknode(fs.root, "c", 1, NULL, &sb);
	TEST_NOT_NULL(c);
	TEST_ASSERT(fs.root->data.children == a);
	TEST_ASSERT(a->next == b);
	TEST_ASSERT(b->next == c);
	TEST_NULL(c->next);

	d = fstree_mknode(fs.root, "d", 1, NULL, &sb);
	TEST_NOT_NULL(d);
	TEST_ASSERT(fs.root->data.children == a);
	TEST_ASSERT(a->next == b);
	TEST_ASSERT(b->next == c);
	TEST_ASSERT(c->next == d);
	TEST_NULL(d->next);

	fstree_cleanup(&fs);

	/* out-of-order */
	ret = fstree_init(&fs, &fsd);
	TEST_EQUAL_I(ret, 0);

	d = fstree_mknode(fs.root, "d", 1, NULL, &sb);
	TEST_NOT_NULL(d);
	TEST_ASSERT(fs.root->data.children == d);
	TEST_NULL(d->next);

	c = fstree_mknode(fs.root, "c", 1, NULL, &sb);
	TEST_NOT_NULL(c);
	TEST_ASSERT(fs.root->data.children == c);
	TEST_ASSERT(c->next == d);
	TEST_NULL(d->next);

	b = fstree_mknode(fs.root, "b", 1, NULL, &sb);
	TEST_NOT_NULL(b);
	TEST_ASSERT(fs.root->data.children == b);
	TEST_ASSERT(b->next == c);
	TEST_ASSERT(c->next == d);
	TEST_NULL(d->next);

	a = fstree_mknode(fs.root, "a", 1, NULL, &sb);
	TEST_NOT_NULL(a);
	TEST_ASSERT(fs.root->data.children == a);
	TEST_ASSERT(a->next == b);
	TEST_ASSERT(b->next == c);
	TEST_ASSERT(c->next == d);
	TEST_NULL(d->next);

	fstree_cleanup(&fs);
	return EXIT_SUCCESS;
}
