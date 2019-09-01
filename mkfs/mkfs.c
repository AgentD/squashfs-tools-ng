/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * mkfs.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "mkfs.h"

static int set_working_dir(options_t *opt)
{
	const char *ptr;

	if (opt->packdir != NULL)
		return pushd(opt->packdir);

	ptr = strrchr(opt->infile, '/');
	if (ptr != NULL)
		return pushdn(opt->infile, ptr - opt->infile);

	return 0;
}

static int restore_working_dir(options_t *opt)
{
	if (opt->packdir != NULL || strrchr(opt->infile, '/') != NULL)
		return popd();

	return 0;
}

static int pack_files(data_writer_t *data, fstree_t *fs, options_t *opt)
{
	file_info_t *fi;
	int ret, infd;

	if (set_working_dir(opt))
		return -1;

	while (fs->files != NULL) {
		fi = fs->files;
		fs->files = fi->next;

		if (!opt->quiet)
			printf("packing %s\n", fi->input_file);

		infd = open(fi->input_file, O_RDONLY);
		if (infd < 0) {
			perror(fi->input_file);
			return -1;
		}

		ret = write_data_from_fd(data, fi, infd, 0);
		close(infd);
		if (ret)
			return -1;
	}

	if (data_writer_sync(data))
		return -1;

	return restore_working_dir(opt);
}

static int read_fstree(fstree_t *fs, options_t *opt)
{
	FILE *fp;
	int ret;

	if (opt->infile == NULL)
		return fstree_from_dir(fs, opt->packdir, opt->dirscan_flags);

	fp = fopen(opt->infile, "rb");
	if (fp == NULL) {
		perror(opt->infile);
		return -1;
	}

	if (set_working_dir(opt)) {
		fclose(fp);
		return -1;
	}

	ret = fstree_from_file(fs, opt->infile, fp);

	fclose(fp);

	if (restore_working_dir(opt))
		return -1;

	return ret;
}

int main(int argc, char **argv)
{
	int status = EXIT_FAILURE, ret;
	compressor_config_t cfg;
	data_writer_t *data;
	sqfs_super_t super;
	compressor_t *cmp;
	id_table_t *idtbl;
	options_t opt;
	fstree_t fs;
	int outfd;

	process_command_line(&opt, argc, argv);

	if (compressor_cfg_init_options(&cfg, opt.compressor,
					opt.blksz, opt.comp_extra)) {
		return EXIT_FAILURE;
	}

	if (fstree_init(&fs, opt.blksz, opt.fs_defaults))
		return EXIT_FAILURE;

	if (sqfs_super_init(&super, opt.blksz, fs.defaults.st_mtime,
			    opt.compressor)) {
		goto out_fstree;
	}

	idtbl = id_table_create();
	if (idtbl == NULL)
		goto out_fstree;

	outfd = open(opt.outfile, opt.outmode, 0644);
	if (outfd < 0) {
		perror(opt.outfile);
		goto out_idtbl;
	}

	if (sqfs_super_write(&super, outfd))
		goto out_outfd;

	if (read_fstree(&fs, &opt))
		goto out_outfd;

	tree_node_sort_recursive(fs.root);

	if (fstree_gen_inode_table(&fs))
		goto out_outfd;

	fstree_gen_file_list(&fs);

	super.inode_count = fs.inode_tbl_size - 2;

#ifdef WITH_SELINUX
	if (opt.selinux != NULL) {
		if (fstree_relabel_selinux(&fs, opt.selinux))
			goto out_outfd;
	}
#endif

	fstree_xattr_deduplicate(&fs);

	cmp = compressor_create(&cfg);
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

	data = data_writer_create(&super, cmp, outfd,
				  opt.devblksz, opt.num_jobs);
	if (data == NULL)
		goto out_cmp;

	if (pack_files(data, &fs, &opt))
		goto out_data;

	if (sqfs_serialize_fstree(outfd, &super, &fs, cmp, idtbl))
		goto out_data;

	if (data_writer_write_fragment_table(data))
		goto out_data;

	if (opt.exportable) {
		if (write_export_table(outfd, &fs, &super, cmp))
			goto out_data;
	}

	if (id_table_write(idtbl, outfd, &super, cmp))
		goto out_data;

	if (write_xattr(outfd, &fs, &super, cmp))
		goto out_data;

	if (sqfs_super_write(&super, outfd))
		goto out_data;

	if (padd_file(outfd, super.bytes_used, opt.devblksz))
		goto out_data;

	if (!opt.quiet) {
		fstree_gen_file_list(&fs);
		sqfs_print_statistics(&fs, &super);
	}

	status = EXIT_SUCCESS;
out_data:
	data_writer_destroy(data);
out_cmp:
	cmp->destroy(cmp);
out_outfd:
	close(outfd);
out_idtbl:
	id_table_destroy(idtbl);
out_fstree:
	fstree_cleanup(&fs);
	return status;
}
