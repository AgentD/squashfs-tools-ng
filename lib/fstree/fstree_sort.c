/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "fstree.h"

#include <string.h>

static tree_node_t *sort_list(tree_node_t *head)
{
	tree_node_t *it, *prev, *lhs, *rhs;
	size_t i, count = 0;

	for (it = head; it != NULL; it = it->next)
		++count;

	if (count < 2)
		return head;

	prev = NULL;
	it = head;

	for (i = 0; i < count / 2; ++i) {
		prev = it;
		it = it->next;
	}

	prev->next = NULL;

	lhs = sort_list(head);
	rhs = sort_list(it);

	head = NULL;
	prev = NULL;

	while (lhs != NULL && rhs != NULL) {
		if (strcmp(lhs->name, rhs->name) <= 0) {
			it = lhs;
			lhs = lhs->next;
		} else {
			it = rhs;
			rhs = rhs->next;
		}

		it->next = NULL;

		if (prev != NULL) {
			prev->next = it;
			prev = it;
		} else {
			prev = head = it;
		}
	}

	it = (lhs != NULL ? lhs : rhs);

	if (prev != NULL) {
		prev->next = it;
	} else {
		head = it;
	}

	return head;
}

static void sort_directory(tree_node_t *n)
{
	n->data.dir->children = sort_list(n->data.dir->children);

	for (n = n->data.dir->children; n != NULL; n = n->next) {
		if (S_ISDIR(n->mode))
			sort_directory(n);
	}
}

void fstree_sort(fstree_t *fs)
{
	sort_directory(fs->root);
}
