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
	int status = EXIT_FAILURE;
	sqfs_info_t info;

	memset(&info, 0, sizeof(info));

	process_command_line(&info.opt, argc, argv);

	if (sqfs_super_init(&info) != 0)
		return EXIT_FAILURE;

	info.outfd = open(info.opt.outfile, info.opt.outmode, 0644);
	if (info.outfd < 0) {
		perror(info.opt.outfile);
		return EXIT_FAILURE;
	}

	if (sqfs_super_write(&info))
		goto out_outfd;

	if (fstree_init(&info.fs, info.opt.blksz, info.opt.def_mtime,
			info.opt.def_mode, info.opt.def_uid,
			info.opt.def_gid)) {
		goto out_outfd;
	}

	if (fstree_from_file(&info.fs, info.opt.infile))
		goto out_fstree;

	fstree_sort(&info.fs);

	print_tree(0, info.fs.root);

	info.cmp = compressor_create(info.super.compression_id, true,
				     info.super.block_size);
	if (info.cmp == NULL) {
		fputs("Error creating compressor\n", stderr);
		goto out_outfd;
	}

	if (write_data_to_image(&info))
		goto out_cmp;

	if (sqfs_super_write(&info))
		goto out_cmp;

	if (sqfs_padd_file(&info))
		goto out_cmp;

	status = EXIT_SUCCESS;
out_cmp:
	info.cmp->destroy(info.cmp);
out_fstree:
	fstree_cleanup(&info.fs);
out_outfd:
	close(info.outfd);
	return status;
}
