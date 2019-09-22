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
	sqfs_inode_generic_t *inode;
	size_t max_blk_count;
	sqfs_file_t *file;
	file_info_t *fi;
	int ret;

	if (set_working_dir(opt))
		return -1;

	for (fi = fs->files; fi != NULL; fi = fi->next) {
		if (!opt->quiet)
			printf("packing %s\n", fi->input_file);

		max_blk_count = fi->size / fs->block_size;
		if (fi->size % fs->block_size)
			++max_blk_count;

		inode = alloc_flex(sizeof(*inode), sizeof(uint32_t),
				   max_blk_count);
		if (inode == NULL) {
			perror("creating file inode");
			return -1;
		}

		inode->block_sizes = (uint32_t *)inode->extra;
		inode->base.type = SQFS_INODE_FILE;
		sqfs_inode_set_file_size(inode, fi->size);
		sqfs_inode_set_frag_location(inode, 0xFFFFFFFF, 0xFFFFFFFF);

		fi->user_ptr = inode;

		file = sqfs_open_file(fi->input_file,
				      SQFS_FILE_OPEN_READ_ONLY);
		if (file == NULL) {
			perror(fi->input_file);
			return -1;
		}

		ret = write_data_from_file(data, inode, file, 0);
		file->destroy(file);

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
	sqfs_compressor_config_t cfg;
	sqfs_compressor_t *cmp;
	sqfs_id_table_t *idtbl;
	sqfs_file_t *outfile;
	data_writer_t *data;
	sqfs_super_t super;
	options_t opt;
	fstree_t fs;

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

	idtbl = sqfs_id_table_create();
	if (idtbl == NULL)
		goto out_fstree;

	outfile = sqfs_open_file(opt.outfile, opt.outmode);
	if (outfile == NULL) {
		perror(opt.outfile);
		goto out_idtbl;
	}

	if (sqfs_super_write(&super, outfile))
		goto out_outfile;

	if (read_fstree(&fs, &opt))
		goto out_outfile;

	tree_node_sort_recursive(fs.root);

	if (fstree_gen_inode_table(&fs))
		goto out_outfile;

	fstree_gen_file_list(&fs);

	super.inode_count = fs.inode_tbl_size - 2;

#ifdef WITH_SELINUX
	if (opt.selinux != NULL) {
		if (fstree_relabel_selinux(&fs, opt.selinux))
			goto out_outfile;
	}
#endif

	fstree_xattr_deduplicate(&fs);

	cmp = sqfs_compressor_create(&cfg);
	if (cmp == NULL) {
		fputs("Error creating compressor\n", stderr);
		goto out_outfile;
	}

	ret = cmp->write_options(cmp, outfile);
	if (ret < 0)
		goto out_cmp;

	if (ret > 0)
		super.flags |= SQFS_FLAG_COMPRESSOR_OPTIONS;

	data = data_writer_create(&super, cmp, outfile,
				  opt.devblksz, opt.num_jobs, opt.max_backlog);
	if (data == NULL)
		goto out_cmp;

	if (pack_files(data, &fs, &opt))
		goto out_data;

	if (sqfs_serialize_fstree(outfile, &super, &fs, cmp, idtbl))
		goto out_data;

	if (data_writer_write_fragment_table(data))
		goto out_data;

	if (opt.exportable) {
		if (write_export_table(outfile, &fs, &super, cmp))
			goto out_data;
	}

	if (sqfs_id_table_write(idtbl, outfile, &super, cmp))
		goto out_data;

	if (write_xattr(outfile, &fs, &super, cmp))
		goto out_data;

	super.bytes_used = outfile->get_size(outfile);

	if (sqfs_super_write(&super, outfile))
		goto out_data;

	if (padd_sqfs(outfile, super.bytes_used, opt.devblksz))
		goto out_data;

	if (!opt.quiet)
		sqfs_print_statistics(&super, data_writer_get_stats(data));

	status = EXIT_SUCCESS;
out_data:
	data_writer_destroy(data);
out_cmp:
	cmp->destroy(cmp);
out_outfile:
	outfile->destroy(outfile);
out_idtbl:
	sqfs_id_table_destroy(idtbl);
out_fstree:
	fstree_cleanup(&fs);
	return status;
}
