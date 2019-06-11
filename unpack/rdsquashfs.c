/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "rdsquashfs.h"

static tree_node_t *find_node(tree_node_t *n, const char *path)
{
	const char *end;
	size_t len;

	while (path != NULL && *path != '\0') {
		if (!S_ISDIR(n->mode)) {
			errno = ENOTDIR;
			return NULL;
		}

		end = strchrnul(path, '/');
		len = end - path;

		for (n = n->data.dir->children; n != NULL; n = n->next) {
			if (strncmp(path, n->name, len) != 0)
				continue;
			if (n->name[len] != '\0')
				continue;
			break;
		}

		if (n == NULL) {
			errno = ENOENT;
			return NULL;
		}

		path = *end ? (end + 1) : end;
	}

	return n;
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

	if ((super.version_major != SQFS_VERSION_MAJOR) ||
	    (super.version_minor != SQFS_VERSION_MINOR)) {
		fprintf(stderr,
			"The image uses squashfs version %d.%d\n"
			"We currently only supports version %d.%d (sorry).\n",
			super.version_major, super.version_minor,
			SQFS_VERSION_MAJOR, SQFS_VERSION_MINOR);
		goto out_fd;
	}

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

	switch (opt.op) {
	case OP_LS:
		n = find_node(fs.root, opt.cmdpath);
		if (n == NULL) {
			perror(opt.cmdpath);
			goto out_fs;
		}

		list_files(n);
		break;
	case OP_CAT:
		n = find_node(fs.root, opt.cmdpath);
		if (n == NULL) {
			perror(opt.cmdpath);
			goto out_fs;
		}

		if (!S_ISREG(n->mode)) {
			fprintf(stderr, "/%s: not a regular file\n",
				opt.cmdpath);
			goto out_fs;
		}

		data = data_reader_create(sqfsfd, &super, cmp);
		if (data == NULL)
			goto out_fs;

		if (data_reader_dump_file(data, n->data.file, STDOUT_FILENO))
			goto out_fs;
		break;
	case OP_UNPACK:
		n = find_node(fs.root, opt.cmdpath);
		if (n == NULL) {
			perror(opt.cmdpath);
			goto out_fs;
		}

		data = data_reader_create(sqfsfd, &super, cmp);
		if (data == NULL)
			goto out_fs;

		if (restore_fstree(opt.unpack_root, n, data, opt.flags))
			goto out_fs;
		break;
	case OP_DESCRIBE:
		describe_tree(fs.root, opt.unpack_root);
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
