/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "fstree.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int fstree_add_xattr(fstree_t *fs, tree_node_t *node,
		     const char *key, const char *value)
{
	tree_xattr_t *xattr, *prev, *it;
	size_t key_idx, value_idx;

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

	if (node->xattr == NULL) {
		xattr = calloc(1, sizeof(*xattr) + sizeof(uint64_t) * 4);
		if (xattr == NULL) {
			perror("adding extended attributes");
			return -1;
		}

		xattr->max_attr = 4;
		xattr->owner = node;

		xattr->next = fs->xattr;
		fs->xattr = xattr;

		node->xattr = xattr;
	} else {
		xattr = node->xattr;

		if (xattr->max_attr == xattr->num_attr) {
			prev = NULL;
			it = fs->xattr;

			while (it != xattr) {
				prev = it;
				it = it->next;
			}

			if (prev == NULL) {
				fs->xattr = xattr->next;
			} else {
				prev->next = xattr->next;
			}

			node->xattr = NULL;

			it = realloc(xattr, sizeof(*xattr) +
				     sizeof(uint64_t) * xattr->max_attr * 2);

			if (it == NULL) {
				perror("adding extended attributes");
				free(xattr);
				return -1;
			}

			xattr = it;
			xattr->max_attr *= 2;

			node->xattr = xattr;
			xattr->next = fs->xattr;
			fs->xattr = xattr;
		}
	}

	xattr->ref[xattr->num_attr]  = (uint64_t)key_idx << 32;
	xattr->ref[xattr->num_attr] |= (uint64_t)value_idx;
	xattr->num_attr += 1;
	return 0;
}

static int cmp_u64(const void *lhs, const void *rhs)
{
	uint64_t l = *((uint64_t *)lhs), r = *((uint64_t *)rhs);

	return l < r ? -1 : (l > r ? 1 : 0);
}

void fstree_xattr_reindex(fstree_t *fs)
{
	tree_xattr_t *it;
	size_t index = 0;

	for (it = fs->xattr; it != NULL; it = it->next)
		it->index = index++;
}

void fstree_xattr_deduplicate(fstree_t *fs)
{
	tree_xattr_t *it, *it1, *prev;
	int diff;

	for (it = fs->xattr; it != NULL; it = it->next)
		qsort(it->ref, it->num_attr, sizeof(it->ref[0]), cmp_u64);

	prev = NULL;
	it = fs->xattr;

	while (it != NULL) {
		for (it1 = fs->xattr; it1 != it; it1 = it1->next) {
			if (it1->num_attr != it->num_attr)
				continue;

			diff = memcmp(it1->ref, it->ref,
				      it->num_attr * sizeof(it->ref[0]));
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
