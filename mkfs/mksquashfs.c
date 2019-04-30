/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "mksquashfs.h"

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
	int outfd, status = EXIT_FAILURE;
	sqfs_super_t super;
	options_t opt;
	fstree_t fs;

	process_command_line(&opt, argc, argv);

	if (sqfs_super_init(&super, &opt) != 0)
		return EXIT_FAILURE;

	outfd = open(opt.outfile, opt.outmode, 0644);
	if (outfd < 0) {
		perror(opt.outfile);
		return EXIT_FAILURE;
	}

	if (sqfs_super_write(&super, outfd))
		goto out_outfd;

	if (fstree_init(&fs, opt.blksz, opt.def_mtime, opt.def_mode,
			opt.def_uid, opt.def_gid)) {
		goto out_outfd;
	}

	if (fstree_from_file(&fs, opt.infile))
		goto out_fstree;

	fstree_sort(&fs);

	print_tree(0, fs.root);

	if (sqfs_super_write(&super, outfd))
		goto out_outfd;

	if (sqfs_padd_file(&super, &opt, outfd))
		goto out_fstree;

	status = EXIT_SUCCESS;
out_fstree:
	fstree_cleanup(&fs);
out_outfd:
	close(outfd);
	return status;
}
