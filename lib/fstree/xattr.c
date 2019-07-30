/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * xattr.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "fstree.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static void remove_from_list(fstree_t *fs, tree_xattr_t *xattr)
{
	tree_xattr_t *prev = NULL, *it = fs->xattr;

	while (it != xattr) {
		prev = it;
		it = it->next;
	}

	if (prev == NULL) {
		fs->xattr = xattr->next;
	} else {
		prev->next = xattr->next;
	}
}

static tree_xattr_t *grow_xattr_block(tree_xattr_t *xattr)
{
	size_t new_size, old_size = 0, new_count = 4;
	void *new;

	if (xattr != NULL) {
		new_count = xattr->max_attr * 2;
		old_size = sizeof(*xattr) + sizeof(uint64_t) * xattr->max_attr;
	}

	new_size = sizeof(*xattr) + sizeof(uint64_t) * new_count;
	new = realloc(xattr, new_size);

	if (new == NULL) {
		perror("adding extended attributes");
		free(xattr);
		return NULL;
	}

	memset((char *)new + old_size, 0, new_size - old_size);

	xattr = new;
	xattr->max_attr = new_count;
	return xattr;
}

int fstree_add_xattr(fstree_t *fs, tree_node_t *node,
		     const char *key, const char *value)
{
	size_t key_idx, value_idx;
	tree_xattr_t *xattr;

	if (str_table_get_index(&fs->xattr_keys, key, &key_idx))
		return -1;

	if (str_table_get_index(&fs->xattr_values, value, &value_idx))
		return -1;

	if (sizeof(size_t) > sizeof(uint32_t)) {
		if (key_idx > 0xFFFFFFFFUL) {
			fputs("Too many unique xattr keys\n", stderr);
			return -1;
		}

		if (value_idx > 0xFFFFFFFFUL) {
			fputs("Too many unique xattr values\n", stderr);
			return -1;
		}
	}

	xattr = node->xattr;

	if (xattr == NULL || xattr->max_attr == xattr->num_attr) {
		if (xattr != NULL)
			remove_from_list(fs, xattr);

		xattr = grow_xattr_block(xattr);
		if (xattr == NULL)
			return -1;

		node->xattr = xattr;
		xattr->owner = node;
		xattr->next = fs->xattr;
		fs->xattr = xattr;
	}

	xattr->attr[xattr->num_attr].key_index = key_idx;
	xattr->attr[xattr->num_attr].value_index = value_idx;
	xattr->num_attr += 1;
	return 0;
}

static int cmp_attr(const void *lhs, const void *rhs)
{
	return memcmp(lhs, rhs, sizeof(((tree_xattr_t *)0)->attr[0]));
}

void fstree_xattr_reindex(fstree_t *fs)
{
	uint32_t key_idx, value_idx;
	size_t i, index = 0;
	tree_xattr_t *it;

	str_table_reset_ref_count(&fs->xattr_keys);
	str_table_reset_ref_count(&fs->xattr_values);

	for (it = fs->xattr; it != NULL; it = it->next) {
		it->index = index++;

		for (i = 0; i < it->num_attr; ++i) {
			key_idx = it->attr[i].key_index;
			value_idx = it->attr[i].value_index;

			str_table_add_ref(&fs->xattr_keys, key_idx);
			str_table_add_ref(&fs->xattr_values, value_idx);
		}
	}
}

void fstree_xattr_deduplicate(fstree_t *fs)
{
	tree_xattr_t *it, *it1, *prev;
	int diff;

	for (it = fs->xattr; it != NULL; it = it->next)
		qsort(it->attr, it->num_attr, sizeof(it->attr[0]), cmp_attr);

	prev = NULL;
	it = fs->xattr;

	while (it != NULL) {
		for (it1 = fs->xattr; it1 != it; it1 = it1->next) {
			if (it1->num_attr != it->num_attr)
				continue;

			diff = memcmp(it1->attr, it->attr,
				      it->num_attr * sizeof(it->attr[0]));
			if (diff == 0)
				break;
		}

		if (it1 != it) {
			prev->next = it->next;
			it->owner->xattr = it1;

			free(it);
			it = prev->next;
		} else {
			prev = it;
			it = it->next;
		}
	}

	fstree_xattr_reindex(fs);
}
