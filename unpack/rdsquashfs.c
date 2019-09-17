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
	sqfs_compressor_config_t cfg;
	int status = EXIT_FAILURE;
	sqfs_compressor_t *cmp;
	data_reader_t *data;
	sqfs_super_t super;
	sqfs_file_t *file;
	tree_node_t *n;
	options_t opt;
	fstree_t fs;

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

	if (super.flags & SQFS_FLAG_NO_XATTRS)
		opt.rdtree_flags &= ~RDTREE_READ_XATTR;

	if (deserialize_fstree(&fs, &super, cmp, file, opt.rdtree_flags))
		goto out_cmp;

	fstree_gen_file_list(&fs);

	data = data_reader_create(file, &super, cmp);
	if (data == NULL)
		goto out_fs;

	if (opt.cmdpath != NULL) {
		n = fstree_node_from_path(&fs, opt.cmdpath);
		if (n == NULL) {
			perror(opt.cmdpath);
			goto out;
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
			goto out;
		}

		if (data_reader_dump_file(data, n->data.file,
					  STDOUT_FILENO, false))
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

		if (fill_unpacked_files(&fs, data, opt.flags))
			goto out;

		if (update_tree_attribs(&fs, n, opt.flags))
			goto out;

		if (opt.unpack_root != NULL && popd() != 0)
			goto out;
		break;
	case OP_DESCRIBE:
		if (describe_tree(fs.root, opt.unpack_root))
			goto out;
		break;
	case OP_RDATTR:
		if (n->xattr != NULL)
			dump_xattrs(&fs, n->xattr);
		break;
	}

	status = EXIT_SUCCESS;
out:
	data_reader_destroy(data);
out_fs:
	fstree_cleanup(&fs);
out_cmp:
	cmp->destroy(cmp);
out_file:
	file->destroy(file);
out_cmd:
	free(opt.cmdpath);
	return status;
}
