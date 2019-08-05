/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * compare_dir.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "difftool.h"

int compare_dir_entries(tree_node_t *a, tree_node_t *b)
{
	tree_node_t *ait = a->data.dir->children, *aprev = NULL;
	tree_node_t *bit = b->data.dir->children, *bprev = NULL;
	int ret, result = 0;
	char *path, arrow;

	while (ait != NULL && bit != NULL) {
		ret = strcmp(ait->name, bit->name);

		if (ret < 0) {
			result = 1;
			path = node_path(ait);
			if (path == NULL)
				return -1;
			fprintf(stdout, "< %s\n", path);
			free(path);

			if (aprev == NULL) {
				a->data.dir->children = ait->next;
				free(ait);
				ait = a->data.dir->children;
			} else {
				aprev->next = ait->next;
				free(ait);
				ait = aprev->next;
			}
		} else if (ret > 0) {
			result = 1;
			path = node_path(bit);
			if (path == NULL)
				return -1;
			fprintf(stdout, "> %s\n", path);
			free(path);

			if (bprev == NULL) {
				b->data.dir->children = bit->next;
				free(bit);
				bit = b->data.dir->children;
			} else {
				bprev->next = bit->next;
				free(bit);
				bit = bprev->next;
			}
		} else {
			aprev = ait;
			ait = ait->next;

			bprev = bit;
			bit = bit->next;
		}
	}

	if (ait != NULL || bit != NULL) {
		result = 1;

		if (ait != NULL) {
			if (aprev == NULL) {
				a->data.dir->children = NULL;
			} else {
				aprev->next = NULL;
			}
			arrow = '<';
		} else {
			if (bprev == NULL) {
				b->data.dir->children = NULL;
			} else {
				bprev->next = NULL;
			}
			arrow = '>';
			ait = bit;
		}

		while (ait != NULL) {
			path = node_path(ait);
			if (path == NULL) {
				result = -1;
			} else {
				fprintf(stdout, "%c %s\n", arrow, path);
				free(path);
			}
			aprev = ait;
			ait = ait->next;
			free(aprev);
		}
	}

	return result;
}
