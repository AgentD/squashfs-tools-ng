/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * mkfs.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "mkfs.h"

static int pack_files(sqfs_block_processor_t *data, fstree_t *fs,
		      options_t *opt)
{
	sqfs_u64 filesize;
	sqfs_file_t *file;
	tree_node_t *node;
	const char *path;
	char *node_path;
	file_info_t *fi;
	int flags;
	int ret;

	if (opt->packdir != NULL && chdir(opt->packdir) != 0) {
		perror(opt->packdir);
		return -1;
	}

	for (fi = fs->files; fi != NULL; fi = fi->next) {
		if (fi->input_file == NULL) {
			node = container_of(fi, tree_node_t, data.file);

			node_path = fstree_get_path(node);
			if (node_path == NULL) {
				perror("reconstructing file path");
				return -1;
			}

			ret = canonicalize_name(node_path);
			assert(ret == 0);

			path = node_path;
		} else {
			node_path = NULL;
			path = fi->input_file;
		}

		if (!opt->cfg.quiet)
			printf("packing %s\n", path);

		file = sqfs_open_file(path, SQFS_FILE_OPEN_READ_ONLY);
		if (file == NULL) {
			perror(path);
			free(node_path);
			return -1;
		}

		flags = 0;
		filesize = file->get_size(file);

		if (opt->no_tail_packing && filesize > opt->cfg.block_size)
			flags |= SQFS_BLK_DONT_FRAGMENT;

		ret = write_data_from_file(path, data, &fi->inode, file, flags);
		sqfs_destroy(file);
		free(node_path);

		if (ret)
			return -1;
	}

	return 0;
}

static int relabel_tree_dfs(const char *filename, sqfs_xattr_writer_t *xwr,
			    tree_node_t *n, void *selinux_handle)
{
	char *path = fstree_get_path(n);
	int ret;

	if (path == NULL) {
		perror("getting absolute node path for SELinux relabeling");
		return -1;
	}

	ret = sqfs_xattr_writer_begin(xwr, 0);
	if (ret) {
		sqfs_perror(filename, "recording xattr key-value pairs", ret);
		return -1;
	}

	if (selinux_relable_node(selinux_handle, xwr, n, path)) {
		free(path);
		return -1;
	}

	ret = sqfs_xattr_writer_end(xwr, &n->xattr_idx);
	if (ret) {
		sqfs_perror(filename, "flushing completed key-value pairs",
			    ret);
		return -1;
	}

	free(path);

	if (S_ISDIR(n->mode)) {
		for (n = n->data.dir.children; n != NULL; n = n->next) {
			if (relabel_tree_dfs(filename, xwr, n, selinux_handle))
				return -1;
		}
	}

	return 0;
}

static int read_fstree(fstree_t *fs, options_t *opt, sqfs_xattr_writer_t *xwr,
		       void *selinux_handle)
{
	int ret;

	ret = fstree_from_file(fs, opt->infile, opt->packdir);

	if (ret == 0 && selinux_handle != NULL)
		ret = relabel_tree_dfs(opt->cfg.filename, xwr,
				       fs->root, selinux_handle);

	return ret;
}

static void override_owner_dfs(const options_t *opt, tree_node_t *n)
{
	if (opt->force_uid)
		n->uid = opt->force_uid_value;

	if (opt->force_gid)
		n->gid = opt->force_gid_value;

	if (S_ISDIR(n->mode)) {
		for (n = n->data.dir.children; n != NULL; n = n->next)
			override_owner_dfs(opt, n);
	}
}

int main(int argc, char **argv)
{
	int status = EXIT_FAILURE;
	void *sehnd = NULL;
	sqfs_writer_t sqfs;
	options_t opt;

	process_command_line(&opt, argc, argv);

	if (sqfs_writer_init(&sqfs, &opt.cfg))
		return EXIT_FAILURE;

	if (opt.selinux != NULL) {
		sehnd = selinux_open_context_file(opt.selinux);
		if (sehnd == NULL)
			goto out;
	}

	if (opt.infile == NULL) {
		if (fstree_from_dir(&sqfs.fs, sqfs.fs.root, opt.packdir,
				    NULL, NULL, opt.dirscan_flags)) {
			goto out;
		}
	} else {
		if (read_fstree(&sqfs.fs, &opt, sqfs.xwr, sehnd))
			goto out;
	}

	if (opt.force_uid || opt.force_gid)
		override_owner_dfs(&opt, sqfs.fs.root);

	if (fstree_post_process(&sqfs.fs))
		goto out;

	if (opt.infile == NULL) {
		if (xattrs_from_dir(&sqfs.fs, opt.packdir, sehnd,
				    sqfs.xwr, opt.scan_xattr)) {
			goto out;
		}
	}

	if (pack_files(sqfs.data, &sqfs.fs, &opt))
		goto out;

	if (sqfs_writer_finish(&sqfs, &opt.cfg))
		goto out;

	status = EXIT_SUCCESS;
out:
	sqfs_writer_cleanup(&sqfs, status);
	if (sehnd != NULL)
		selinux_close_context_file(sehnd);
	free(opt.packdir);
	return status;
}
