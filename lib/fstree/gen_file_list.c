/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include "fstree.h"

static file_info_t *file_list_dfs(tree_node_t *n)
{
	if (S_ISREG(n->mode))
		return n->data.file;

	if (S_ISDIR(n->mode)) {
		file_info_t *list = NULL, *last = NULL;

		for (n = n->data.dir->children; n != NULL; n = n->next) {
			if (list == NULL) {
				list = file_list_dfs(n);
				if (list == NULL)
					continue;
				last = list;
			} else {
				last->next = file_list_dfs(n);
			}

			while (last->next != NULL)
				last = last->next;
		}

		return list;
	}

	return NULL;
}

void fstree_gen_file_list(fstree_t *fs)
{
	fs->files = file_list_dfs(fs->root);
}
