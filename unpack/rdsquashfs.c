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
	sqfs_reader_t sqfs;
	tree_node_t *n;
	options_t opt;

	process_command_line(&opt, argc, argv);

	if (sqfs_reader_open(&sqfs, opt.image_name, opt.rdtree_flags))
		goto out_cmd;

	if (opt.cmdpath != NULL) {
		n = fstree_node_from_path(&sqfs.fs, opt.cmdpath);
		if (n == NULL) {
			perror(opt.cmdpath);
			goto out_fs;
		}
	} else {
		n = sqfs.fs.root;
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

		if (data_reader_dump_file(sqfs.data, n->data.file,
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

		if (fill_unpacked_files(&sqfs.fs, sqfs.data,
					opt.flags, opt.num_jobs))
			goto out_fs;

		if (update_tree_attribs(&sqfs.fs, n, opt.flags))
			goto out_fs;

		if (opt.unpack_root != NULL && popd() != 0)
			goto out_fs;
		break;
	case OP_DESCRIBE:
		if (describe_tree(sqfs.fs.root, opt.unpack_root))
			goto out_fs;
		break;
	case OP_RDATTR:
		if (n->xattr != NULL)
			dump_xattrs(&sqfs.fs, n->xattr);
		break;
	}

	status = EXIT_SUCCESS;
out_fs:
	sqfs_reader_close(&sqfs);
out_cmd:
	free(opt.cmdpath);
	return status;
}
