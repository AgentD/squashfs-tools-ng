/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "squashfs.h"
#include "options.h"
#include "fstree.h"

#include <sys/sysmacros.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>

static void print_tree(int level, tree_node_t *node)
{
	tree_node_t *n;
	int i;

	for (i = 1; i < level; ++i) {
		fputs("|  ", stdout);
	}

	if (level)
		fputs("+- ", stdout);

	if (S_ISDIR(node->mode)) {
		fprintf(stdout, "%s/ (%u, %u, 0%o)\n", node->name,
			node->uid, node->gid, node->mode & 07777);

		for (n = node->data.dir->children; n != NULL; n = n->next) {
			print_tree(level + 1, n);
		}

		if (node->data.dir->children != NULL) {
			for (i = 0; i < level; ++i)
				fputs("|  ", stdout);

			if (level)
				fputc('\n', stdout);
		}
	} else {
		fprintf(stdout, "%s (%u, %u, 0%o)\n", node->name,
			node->uid, node->gid, node->mode & 07777);
	}
}


int main(int argc, char **argv)
{
	options_t opt;
	fstree_t fs;

	process_command_line(&opt, argc, argv);

	if (fstree_init(&fs, opt.blksz, opt.def_mtime, opt.def_mode,
			opt.def_uid, opt.def_gid)) {
		return EXIT_FAILURE;
	}

	if (fstree_from_file(&fs, opt.infile)) {
		fstree_cleanup(&fs);
		return EXIT_FAILURE;
	}

	fstree_sort(&fs);

	print_tree(0, fs.root);

	fstree_cleanup(&fs);
	return EXIT_SUCCESS;
}
