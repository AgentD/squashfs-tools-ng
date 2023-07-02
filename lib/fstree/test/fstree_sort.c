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

static tree_node_t *mkentry(fstree_t *fs, const char *name)
{
	sqfs_dir_entry_t *ent = sqfs_dir_entry_create(name, S_IFBLK | 0600, 0);
	tree_node_t *out;

	TEST_NOT_NULL(ent);
	ent->rdev = 1337;

	out = fstree_add_generic(fs, ent, NULL);
	free(ent);
	TEST_NOT_NULL(out);

	return out;
}

int main(int argc, char **argv)
{
	tree_node_t *a, *b, *c, *d;
	fstree_defaults_t fsd;
	fstree_t fs;
	int ret;
	(void)argc; (void)argv;

	/* in order */
	TEST_ASSERT(parse_fstree_defaults(&fsd, NULL) == 0);
	ret = fstree_init(&fs, &fsd);
	TEST_EQUAL_I(ret, 0);

	a = mkentry(&fs, "a");
	TEST_NOT_NULL(a);
	TEST_ASSERT(fs.root->data.children == a);
	TEST_NULL(a->next);

	b = mkentry(&fs, "b");
	TEST_NOT_NULL(a);
	TEST_ASSERT(fs.root->data.children == a);
	TEST_ASSERT(a->next == b);
	TEST_NULL(b->next);

	c = mkentry(&fs, "c");
	TEST_NOT_NULL(c);
	TEST_ASSERT(fs.root->data.children == a);
	TEST_ASSERT(a->next == b);
	TEST_ASSERT(b->next == c);
	TEST_NULL(c->next);

	d = mkentry(&fs, "d");
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

	d = mkentry(&fs, "d");
	TEST_NOT_NULL(d);
	TEST_ASSERT(fs.root->data.children == d);
	TEST_NULL(d->next);

	c = mkentry(&fs, "c");
	TEST_NOT_NULL(c);
	TEST_ASSERT(fs.root->data.children == c);
	TEST_ASSERT(c->next == d);
	TEST_NULL(d->next);

	b = mkentry(&fs, "b");
	TEST_NOT_NULL(b);
	TEST_ASSERT(fs.root->data.children == b);
	TEST_ASSERT(b->next == c);
	TEST_ASSERT(c->next == d);
	TEST_NULL(d->next);

	a = mkentry(&fs, "a");
	TEST_NOT_NULL(a);
	TEST_ASSERT(fs.root->data.children == a);
	TEST_ASSERT(a->next == b);
	TEST_ASSERT(b->next == c);
	TEST_ASSERT(c->next == d);
	TEST_NULL(d->next);

	fstree_cleanup(&fs);
	return EXIT_SUCCESS;
}
