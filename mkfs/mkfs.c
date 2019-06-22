/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "mkfs.h"

static int process_file(data_writer_t *data, tree_node_t *n, bool quiet)
{
	int ret, infd;
	char *name;

	if (!quiet) {
		name = fstree_get_path(n);
		printf("packing %s\n", name);
		free(name);
	}

	infd = open(n->data.file->input_file, O_RDONLY);
	if (infd < 0) {
		perror(n->data.file->input_file);
		return -1;
	}

	ret = write_data_from_fd(data, n->data.file, infd);

	close(infd);
	return ret;
}

static int pack_files_dfs(data_writer_t *data, tree_node_t *n, bool quiet)
{
	if (S_ISREG(n->mode))
		return process_file(data, n, quiet);

	if (S_ISDIR(n->mode)) {
		for (n = n->data.dir->children; n != NULL; n = n->next) {
			if (pack_files_dfs(data, n, quiet))
				return -1;
		}
	}

	return 0;
}

static int pack_files(data_writer_t *data, fstree_t *fs, options_t *opt)
{
	bool need_restore = false;
	const char *ptr;

	if (opt->packdir != NULL) {
		if (pushd(opt->packdir))
			return -1;
		need_restore = true;
	} else {
		ptr = strrchr(opt->infile, '/');

		if (ptr != NULL) {
			if (pushdn(opt->infile, ptr - opt->infile))
				return -1;

			need_restore = true;
		}
	}

	if (pack_files_dfs(data, fs->root, opt->quiet))
		return -1;

	if (data_writer_flush_fragments(data))
		return -1;

	return need_restore ? popd() : 0;
}

int main(int argc, char **argv)
{
	int status = EXIT_FAILURE, ret;
	data_writer_t *data;
	sqfs_super_t super;
	compressor_t *cmp;
	id_table_t idtbl;
	options_t opt;
	fstree_t fs;
	int outfd;

	process_command_line(&opt, argc, argv);

	if (sqfs_super_init(&super, opt.blksz, opt.def_mtime, opt.compressor))
		return EXIT_FAILURE;

	if (id_table_init(&idtbl))
		return EXIT_FAILURE;

	outfd = open(opt.outfile, opt.outmode, 0644);
	if (outfd < 0) {
		perror(opt.outfile);
		goto out_idtbl;
	}

	if (sqfs_super_write(&super, outfd))
		goto out_outfd;

	if (fstree_init(&fs, opt.blksz, opt.def_mtime, opt.def_mode,
			opt.def_uid, opt.def_gid)) {
		goto out_outfd;
	}

	if (opt.infile != NULL) {
		if (fstree_from_file(&fs, opt.infile, opt.packdir))
			goto out_fstree;
	} else {
		if (fstree_from_dir(&fs, opt.packdir))
			goto out_fstree;
	}

	tree_node_sort_recursive(fs.root);

	if (fstree_gen_inode_table(&fs))
		goto out_fstree;

	super.inode_count = fs.inode_tbl_size - 2;

#ifdef WITH_SELINUX
	if (opt.selinux != NULL) {
		if (fstree_relabel_selinux(&fs, opt.selinux))
			goto out_fstree;
	}
#endif

	fstree_xattr_deduplicate(&fs);

	cmp = compressor_create(super.compression_id, true, super.block_size,
				opt.comp_extra);
	if (cmp == NULL) {
		fputs("Error creating compressor\n", stderr);
		goto out_outfd;
	}

	ret = cmp->write_options(cmp, outfd);
	if (ret < 0)
		goto out_cmp;

	if (ret > 0) {
		super.flags |= SQFS_FLAG_COMPRESSOR_OPTIONS;
		super.bytes_used += ret;
	}

	data = data_writer_create(&super, cmp, outfd);
	if (data == NULL)
		goto out_cmp;

	if (pack_files(data, &fs, &opt))
		goto out_data;

	if (sqfs_serialize_fstree(outfd, &super, &fs, cmp, &idtbl))
		goto out_data;

	if (data_writer_write_fragment_table(data))
		goto out_data;

	if (id_table_write(&idtbl, outfd, &super, cmp))
		goto out_data;

	if (write_xattr(outfd, &fs, &super, cmp))
		goto out_data;

	if (sqfs_super_write(&super, outfd))
		goto out_data;

	if (padd_file(outfd, super.bytes_used, opt.devblksz))
		goto out_data;

	status = EXIT_SUCCESS;
out_data:
	data_writer_destroy(data);
out_cmp:
	cmp->destroy(cmp);
out_fstree:
	fstree_cleanup(&fs);
out_outfd:
	close(outfd);
out_idtbl:
	id_table_cleanup(&idtbl);
	return status;
}
