/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * fstree_sort.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "fstree.h"
#include "internal.h"
#include "../test.h"

int main(void)
{
	tree_node_t *a, *b, *c, *d;
	struct stat sb;
	fstree_t fs;

	memset(&fs, 0, sizeof(fs));
	memset(&sb, 0, sizeof(sb));
	sb.st_mode = S_IFBLK | 0600;
	sb.st_rdev = 1337;

	a = fstree_mknode(NULL, "a", 1, NULL, &sb);
	b = fstree_mknode(NULL, "b", 1, NULL, &sb);
	c = fstree_mknode(NULL, "c", 1, NULL, &sb);
	d = fstree_mknode(NULL, "d", 1, NULL, &sb);
	TEST_ASSERT(a != NULL && b != NULL && c != NULL && d != NULL);

	/* empty list */
	TEST_NULL(tree_node_list_sort(NULL));

	/* single element */
	TEST_ASSERT(tree_node_list_sort(a) == a);
	TEST_NULL(a->next);

	/* two elements, reverse order */
	b->next = a;
	TEST_ASSERT(tree_node_list_sort(b) == a);
	TEST_ASSERT(a->next == b);
	TEST_NULL(b->next);

	/* two elements, sorted order */
	TEST_ASSERT(tree_node_list_sort(a) == a);
	TEST_ASSERT(a->next == b);
	TEST_NULL(b->next);

	/* three elements, reverse order */
	c->next = b;
	b->next = a;
	a->next = NULL;

	TEST_ASSERT(tree_node_list_sort(c) == a);
	TEST_ASSERT(a->next == b);
	TEST_ASSERT(b->next == c);
	TEST_NULL(c->next);

	/* three elements, ordered */
	TEST_ASSERT(tree_node_list_sort(a) == a);
	TEST_ASSERT(a->next == b);
	TEST_ASSERT(b->next == c);
	TEST_NULL(c->next);

	/* four elements, reverse order */
	d->next = c;
	c->next = b;
	b->next = a;
	a->next = NULL;

	TEST_ASSERT(tree_node_list_sort(d) == a);
	TEST_ASSERT(a->next == b);
	TEST_ASSERT(b->next == c);
	TEST_ASSERT(c->next == d);
	TEST_NULL(d->next);

	/* four elements, sorted order */
	TEST_ASSERT(tree_node_list_sort(a) == a);
	TEST_ASSERT(a->next == b);
	TEST_ASSERT(b->next == c);
	TEST_ASSERT(c->next == d);
	TEST_NULL(d->next);

	/* force merge sort to go through LRLR pattern */
	b->next = a;
	a->next = d;
	d->next = c;
	c->next = NULL;

	TEST_ASSERT(tree_node_list_sort(b) == a);
	TEST_ASSERT(a->next == b);
	TEST_ASSERT(b->next == c);
	TEST_ASSERT(c->next == d);
	TEST_NULL(d->next);

	/* cleanup and done */
	free(a);
	free(b);
	free(c);
	free(d);
	return EXIT_SUCCESS;
}
