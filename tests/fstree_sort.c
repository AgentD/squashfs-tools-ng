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

	memset(&fs, 0, sizeof(fs));
	memset(&sb, 0, sizeof(sb));
	sb.st_mode = S_IFBLK | 0600;
	sb.st_rdev = 1337;

	a = fstree_mknode(&fs, NULL, "a", 1, NULL, &sb);
	b = fstree_mknode(&fs, NULL, "b", 1, NULL, &sb);
	c = fstree_mknode(&fs, NULL, "c", 1, NULL, &sb);
	d = fstree_mknode(&fs, NULL, "d", 1, NULL, &sb);
	assert(a != NULL && b != NULL && c != NULL && d != NULL);

	/* empty list */
	assert(tree_node_list_sort(NULL) == NULL);

	/* single element */
	assert(tree_node_list_sort(a) == a);
	assert(a->next == NULL);

	/* two elements, reverse order */
	b->next = a;
	assert(tree_node_list_sort(b) == a);
	assert(a->next == b);
	assert(b->next == NULL);

	/* two elements, sorted order */
	assert(tree_node_list_sort(a) == a);
	assert(a->next == b);
	assert(b->next == NULL);

	/* three elements, reverse order */
	c->next = b;
	b->next = a;
	a->next = NULL;

	assert(tree_node_list_sort(c) == a);
	assert(a->next == b);
	assert(b->next == c);
	assert(c->next == NULL);

	/* three elements, ordered */
	assert(tree_node_list_sort(a) == a);
	assert(a->next == b);
	assert(b->next == c);
	assert(c->next == NULL);

	/* four elements, reverse order */
	d->next = c;
	c->next = b;
	b->next = a;
	a->next = NULL;

	assert(tree_node_list_sort(d) == a);
	assert(a->next == b);
	assert(b->next == c);
	assert(c->next == d);
	assert(d->next == NULL);

	/* four elements, sorted order */
	assert(tree_node_list_sort(a) == a);
	assert(a->next == b);
	assert(b->next == c);
	assert(c->next == d);
	assert(d->next == NULL);

	/* force merge sort to go through LRLR pattern */
	b->next = a;
	a->next = d;
	d->next = c;
	c->next = NULL;

	assert(tree_node_list_sort(b) == a);
	assert(a->next == b);
	assert(b->next == c);
	assert(c->next == d);
	assert(d->next == NULL);

	/* cleanup and done */
	free(a);
	free(b);
	free(c);
	free(d);
	return EXIT_SUCCESS;
}
