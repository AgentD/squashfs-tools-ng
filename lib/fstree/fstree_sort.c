/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * fstree_sort.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 * Copyright (C) 2019 Zachary Dremann <dremann@gmail.com>
 */
#include "config.h"

#include "fstree.h"

#include <string.h>

static tree_node_t *merge(tree_node_t *lhs, tree_node_t *rhs)
{
	tree_node_t *it;
	tree_node_t *head = NULL;
	tree_node_t **next_ptr = &head;

	while (lhs != NULL && rhs != NULL) {
		if (strcmp(lhs->name, rhs->name) <= 0) {
			it = lhs;
			lhs = lhs->next;
		} else {
			it = rhs;
			rhs = rhs->next;
		}

		*next_ptr = it;
		next_ptr = &it->next;
	}

	it = (lhs != NULL ? lhs : rhs);
	*next_ptr = it;
	return head;
}

tree_node_t *tree_node_list_sort(tree_node_t *head)
{
	tree_node_t *it, *half, *prev;

	it = half = prev = head;
	while (it != NULL) {
		prev = half;
		half = half->next;
		it = it->next;
		if (it != NULL) {
			it = it->next;
		}
	}

	// half refers to the (count/2)'th element ROUNDED UP.
	// It will be null therefore only in the empty and the
	// single element list
	if (half == NULL) {
		return head;
	}

	prev->next = NULL;

	return merge(tree_node_list_sort(head), tree_node_list_sort(half));
}
