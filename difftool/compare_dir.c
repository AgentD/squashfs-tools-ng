/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * compare_dir.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "sqfsdiff.h"

static int print_omitted(sqfsdiff_t *sd, bool is_old, sqfs_tree_node_t *n)
{
	char *path = node_path(n);

	if (path == NULL)
		return -1;

	fprintf(stdout, "%c %s\n", is_old ? '<' : '>', path);

	if ((sd->compare_flags & COMPARE_EXTRACT_FILES) &&
	    S_ISREG(n->inode->base.mode)) {
		if (extract_files(sd, is_old ? n->inode : NULL,
				  is_old ? NULL : n->inode, path)) {
			free(path);
			return -1;
		}
	}

	free(path);

	for (n = n->children; n->children != NULL; n = n->next) {
		if (print_omitted(sd, is_old, n))
			return -1;
	}

	return 0;
}

int compare_dir_entries(sqfsdiff_t *sd, sqfs_tree_node_t *old,
			sqfs_tree_node_t *new)
{
	sqfs_tree_node_t *old_it = old->children, *old_prev = NULL;
	sqfs_tree_node_t *new_it = new->children, *new_prev = NULL;
	int ret, result = 0;

	while (old_it != NULL || new_it != NULL) {
		if (old_it != NULL && new_it != NULL) {
			ret = strcmp((const char *)old_it->name,
				     (const char *)new_it->name);
		} else if (old_it == NULL) {
			ret = 1;
		} else {
			ret = -1;
		}

		if (ret < 0) {
			result = 1;

			if (print_omitted(sd, true, old_it))
				return -1;

			if (old_prev == NULL) {
				old->children = old_it->next;
				sqfs_dir_tree_destroy(old_it);
				old_it = old->children;
			} else {
				old_prev->next = old_it->next;
				sqfs_dir_tree_destroy(old_it);
				old_it = old_prev->next;
			}
		} else if (ret > 0) {
			result = 1;

			if (print_omitted(sd, false, new_it))
				return -1;

			if (new_prev == NULL) {
				new->children = new_it->next;
				sqfs_dir_tree_destroy(new_it);
				new_it = new->children;
			} else {
				new_prev->next = new_it->next;
				sqfs_dir_tree_destroy(new_it);
				new_it = new_prev->next;
			}
		} else {
			old_prev = old_it;
			old_it = old_it->next;

			new_prev = new_it;
			new_it = new_it->next;
		}
	}

	return result;
}
