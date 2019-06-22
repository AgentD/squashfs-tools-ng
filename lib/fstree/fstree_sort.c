/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "fstree.h"

#include <string.h>

static tree_node_t *merge(tree_node_t *lhs, tree_node_t *rhs)
{
	tree_node_t *head = NULL, *prev = NULL, *it;

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

tree_node_t *tree_node_list_sort(tree_node_t *head)
{
	tree_node_t *it, *prev;
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

	return merge(tree_node_list_sort(head), tree_node_list_sort(it));
}

void tree_node_sort_recursive(tree_node_t *n)
{
	n->data.dir->children = tree_node_list_sort(n->data.dir->children);

	for (n = n->data.dir->children; n != NULL; n = n->next) {
		if (S_ISDIR(n->mode))
			tree_node_sort_recursive(n);
	}
}
