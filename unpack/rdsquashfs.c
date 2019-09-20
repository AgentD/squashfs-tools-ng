/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * rdsquashfs.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "rdsquashfs.h"

int main(int argc, char **argv)
{
	sqfs_xattr_reader_t *xattr = NULL;
	sqfs_compressor_config_t cfg;
	int status = EXIT_FAILURE;
	sqfs_dir_reader_t *dirrd;
	sqfs_compressor_t *cmp;
	sqfs_id_table_t *idtbl;
	sqfs_tree_node_t *n;
	data_reader_t *data;
	sqfs_super_t super;
	sqfs_file_t *file;
	options_t opt;
	int ret;

	process_command_line(&opt, argc, argv);

	file = sqfs_open_file(opt.image_name, SQFS_FILE_OPEN_READ_ONLY);
	if (file == NULL) {
		perror(opt.image_name);
		goto out_cmd;
	}

	if (sqfs_super_read(&super, file)) {
		fprintf(stderr, "%s: error reading super block.\n",
			opt.image_name);
		goto out_file;
	}

	if (!sqfs_compressor_exists(super.compression_id)) {
		fprintf(stderr, "%s: unknown compressor used.\n",
			opt.image_name);
		goto out_file;
	}

	sqfs_compressor_config_init(&cfg, super.compression_id,
				    super.block_size,
				    SQFS_COMP_FLAG_UNCOMPRESS);

	cmp = sqfs_compressor_create(&cfg);
	if (cmp == NULL)
		goto out_file;

	if (super.flags & SQFS_FLAG_COMPRESSOR_OPTIONS) {
		if (cmp->read_options(cmp, file))
			goto out_cmp;
	}

	if (!(super.flags & SQFS_FLAG_NO_XATTRS)) {
		xattr = sqfs_xattr_reader_create(file, &super, cmp);

		if (sqfs_xattr_reader_load_locations(xattr)) {
			fputs("Error loading xattr table\n", stderr);
			goto out_xr;
		}
	}

	idtbl = sqfs_id_table_create();
	if (idtbl == NULL) {
		perror("creating ID table");
		goto out_xr;
	}

	if (sqfs_id_table_read(idtbl, file, &super, cmp)) {
		fputs("error loading ID table\n", stderr);
		goto out_id;
	}

	dirrd = sqfs_dir_reader_create(&super, cmp, file);
	if (dirrd == NULL) {
		perror("creating dir reader");
		goto out_id;
	}

	data = data_reader_create(file, &super, cmp);
	if (data == NULL)
		goto out_dr;

	ret = sqfs_dir_reader_get_full_hierarchy(dirrd, idtbl, opt.cmdpath,
						 opt.rdtree_flags, &n);
	if (ret) {
		fprintf(stderr, "error reading hierarchy: %d\n", ret);
		goto out_data;
	}

	switch (opt.op) {
	case OP_LS:
		list_files(n);
		break;
	case OP_CAT:
		if (!S_ISREG(n->inode->base.mode)) {
			fprintf(stderr, "/%s: not a regular file\n",
				opt.cmdpath);
			goto out;
		}

		if (data_reader_dump(data, n->inode, STDOUT_FILENO,
				     super.block_size, false))
			goto out;
		break;
	case OP_UNPACK:
		if (opt.unpack_root != NULL) {
			if (mkdir_p(opt.unpack_root))
				return -1;

			if (pushd(opt.unpack_root))
				return -1;
		}

		if (restore_fstree(n, opt.flags))
			goto out;

		if (fill_unpacked_files(super.block_size, n, data, opt.flags))
			goto out;

		if (update_tree_attribs(xattr, n, opt.flags))
			goto out;

		if (opt.unpack_root != NULL && popd() != 0)
			goto out;
		break;
	case OP_DESCRIBE:
		if (describe_tree(n, opt.unpack_root))
			goto out;
		break;
	case OP_RDATTR:
		if (dump_xattrs(xattr, n->inode))
			goto out;
		break;
	}

	status = EXIT_SUCCESS;
out:
	sqfs_dir_tree_destroy(n);
out_data:
	data_reader_destroy(data);
out_dr:
	sqfs_dir_reader_destroy(dirrd);
out_id:
	sqfs_id_table_destroy(idtbl);
out_xr:
	if (xattr != NULL)
		sqfs_xattr_reader_destroy(xattr);
out_cmp:
	cmp->destroy(cmp);
out_file:
	file->destroy(file);
out_cmd:
	free(opt.cmdpath);
	return status;
}
