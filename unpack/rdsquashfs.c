/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * rdsquashfs.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "rdsquashfs.h"

static void dump_xattrs(fstree_t *fs, tree_xattr_t *xattr)
{
	const char *key, *value;
	size_t i;

	for (i = 0; i < xattr->num_attr; ++i) {
		key = str_table_get_string(&fs->xattr_keys,
					   xattr->attr[i].key_index);
		value = str_table_get_string(&fs->xattr_values,
					     xattr->attr[i].value_index);

		printf("%s=%s\n", key, value);
	}
}

int main(int argc, char **argv)
{
	int status = EXIT_FAILURE;
	data_reader_t *data = NULL;
	sqfs_super_t super;
	compressor_t *cmp;
	tree_node_t *n;
	options_t opt;
	fstree_t fs;
	int sqfsfd;

	process_command_line(&opt, argc, argv);

	sqfsfd = open(opt.image_name, O_RDONLY);
	if (sqfsfd < 0) {
		perror(opt.image_name);
		goto out_cmd;
	}

	if (sqfs_super_read(&super, sqfsfd))
		goto out_fd;

	if (!compressor_exists(super.compression_id)) {
		fputs("Image uses a compressor that has not been built in\n",
		      stderr);
		goto out_fd;
	}

	cmp = compressor_create(super.compression_id, false,
				super.block_size, NULL);
	if (cmp == NULL)
		goto out_fd;

	if (super.flags & SQFS_FLAG_COMPRESSOR_OPTIONS) {
		if (cmp->read_options(cmp, sqfsfd))
			goto out_cmp;
	}

	if (deserialize_fstree(&fs, &super, cmp, sqfsfd, opt.rdtree_flags))
		goto out_cmp;

	fstree_gen_file_list(&fs);

	if (opt.cmdpath != NULL) {
		n = fstree_node_from_path(&fs, opt.cmdpath);
		if (n == NULL) {
			perror(opt.cmdpath);
			goto out_fs;
		}
	} else {
		n = fs.root;
	}

	switch (opt.op) {
	case OP_LS:
		list_files(n);
		break;
	case OP_CAT:
		if (!S_ISREG(n->mode)) {
			fprintf(stderr, "/%s: not a regular file\n",
				opt.cmdpath);
			goto out_fs;
		}

		data = data_reader_create(sqfsfd, &super, cmp);
		if (data == NULL)
			goto out_fs;

		if (data_reader_dump_file(data, n->data.file,
					  STDOUT_FILENO, false))
			goto out_fs;
		break;
	case OP_UNPACK:
		if (opt.unpack_root != NULL) {
			if (mkdir_p(opt.unpack_root))
				return -1;

			if (pushd(opt.unpack_root))
				return -1;
		}

		if (restore_fstree(n, opt.flags))
			goto out_fs;

		data = data_reader_create(sqfsfd, &super, cmp);
		if (data == NULL)
			goto out_fs;

		if (fill_unpacked_files(&fs, data, opt.flags, opt.num_jobs))
			goto out_fs;

		if (update_tree_attribs(&fs, n, opt.flags))
			goto out_fs;

		if (opt.unpack_root != NULL && popd() != 0)
			goto out_fs;
		break;
	case OP_DESCRIBE:
		describe_tree(fs.root, opt.unpack_root);
		break;
	case OP_RDATTR:
		if (n->xattr != NULL)
			dump_xattrs(&fs, n->xattr);
		break;
	}

	status = EXIT_SUCCESS;
out_fs:
	if (data != NULL)
		data_reader_destroy(data);
	fstree_cleanup(&fs);
out_cmp:
	cmp->destroy(cmp);
out_fd:
	close(sqfsfd);
out_cmd:
	free(opt.cmdpath);
	return status;
}
