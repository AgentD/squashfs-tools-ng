/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * compare_dir.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "sqfsdiff.h"

int compare_dir_entries(sqfsdiff_t *sd, tree_node_t *old, tree_node_t *new)
{
	tree_node_t *old_it = old->data.dir->children, *old_prev = NULL;
	tree_node_t *new_it = new->data.dir->children, *new_prev = NULL;
	int ret, result = 0;
	char *path;

	while (old_it != NULL || new_it != NULL) {
		if (old_it != NULL && new_it != NULL) {
			ret = strcmp(old_it->name, new_it->name);
		} else if (old_it == NULL) {
			ret = 1;
		} else {
			ret = -1;
		}

		if (ret < 0) {
			result = 1;
			path = node_path(old_it);
			if (path == NULL)
				return -1;

			if ((sd->compare_flags & COMPARE_EXTRACT_FILES) &&
			    S_ISREG(old_it->mode)) {
				if (extract_files(sd, old_it->data.file,
						  NULL, path)) {
					return -1;
				}
			}

			fprintf(stdout, "< %s\n", path);
			free(path);

			if (old_prev == NULL) {
				old->data.dir->children = old_it->next;
				free(old_it);
				old_it = old->data.dir->children;
			} else {
				old_prev->next = old_it->next;
				free(old_it);
				old_it = old_prev->next;
			}
		} else if (ret > 0) {
			result = 1;
			path = node_path(new_it);
			if (path == NULL)
				return -1;

			if ((sd->compare_flags & COMPARE_EXTRACT_FILES) &&
			    S_ISREG(new_it->mode)) {
				if (extract_files(sd, NULL, new_it->data.file,
						  path)) {
					return -1;
				}
			}

			fprintf(stdout, "> %s\n", path);
			free(path);

			if (new_prev == NULL) {
				new->data.dir->children = new_it->next;
				free(new_it);
				new_it = new->data.dir->children;
			} else {
				new_prev->next = new_it->next;
				free(new_it);
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
