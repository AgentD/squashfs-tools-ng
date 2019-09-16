/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * sqfs2tar.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "sqfs/meta_reader.h"
#include "sqfs/compress.h"
#include "data_reader.h"
#include "highlevel.h"
#include "fstree.h"
#include "util.h"
#include "tar.h"

#include <getopt.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>

static struct option long_opts[] = {
	{ "subdir", required_argument, NULL, 'd' },
	{ "keep-as-dir", no_argument, NULL, 'k' },
	{ "no-skip", no_argument, NULL, 's' },
	{ "no-xattr", no_argument, NULL, 'X' },
	{ "help", no_argument, NULL, 'h' },
	{ "version", no_argument, NULL, 'V' },
};

static const char *short_opts = "d:ksXhV";

static const char *usagestr =
"Usage: sqfs2tar [OPTIONS...] <sqfsfile>\n"
"\n"
"Read an input squashfs archive and turn it into a tar archive, written\n"
"to stdout.\n"
"\n"
"Possible options:\n"
"\n"
"  --subdir, -d <dir>        Unpack the given sub directory instead of the\n"
"                            filesystem root. Can be specified more than\n"
"                            once to select multiple directories. If only\n"
"                            one is specified, it becomes the new root of\n"
"                            node of the archive file system tree.\n"
"\n"
"  --keep-as-dir, -k         If --subdir is used only once, don't make the\n"
"                            subdir the archive root, instead keep it as\n"
"                            prefix for all unpacked files.\n"
"                            Using --subdir more than once implies\n"
"                            --keep-as-dir.\n"
"  --no-xattr, -X            Do not copy extended attributes.\n"
"\n"
"  --no-skip, -s             Abort if a file cannot be stored in a tar\n"
"                            archive. By default, it is simply skipped\n"
"                            and a warning is written to stderr.\n"
"\n"
"  --help, -h                Print help text and exit.\n"
"  --version, -V             Print version information and exit.\n"
"\n"
"Examples:\n"
"\n"
"\tsqfs2tar rootfs.sqfs > rootfs.tar\n"
"\tsqfs2tar rootfs.sqfs | gzip > rootfs.tar.gz\n"
"\tsqfs2tar rootfs.sqfs | xz > rootfs.tar.xz\n"
"\n";

static const char *filename;
static unsigned int record_counter;
static bool dont_skip = false;
static bool keep_as_dir = false;
static bool no_xattr = false;

static const char *current_subdir = NULL;

static char **subdirs = NULL;
static size_t num_subdirs = 0;
static size_t max_subdirs = 0;

static void process_args(int argc, char **argv)
{
	size_t idx, new_count;
	int i, ret;
	void *new;

	for (;;) {
		i = getopt_long(argc, argv, short_opts, long_opts, NULL);
		if (i == -1)
			break;

		switch (i) {
		case 'd':
			if (num_subdirs == max_subdirs) {
				new_count = max_subdirs ? max_subdirs * 2 : 16;
				new = realloc(subdirs,
					      new_count * sizeof(subdirs[0]));
				if (new == NULL)
					goto fail_errno;

				max_subdirs = new_count;
				subdirs = new;
			}

			subdirs[num_subdirs] = strdup(optarg);
			if (subdirs[num_subdirs] == NULL)
				goto fail_errno;

			if (canonicalize_name(subdirs[num_subdirs])) {
				perror(optarg);
				goto fail;
			}

			++num_subdirs;
			break;
		case 'k':
			keep_as_dir = true;
			break;
		case 's':
			dont_skip = true;
			break;
		case 'X':
			no_xattr = true;
			break;
		case 'h':
			fputs(usagestr, stdout);
			goto out_success;
		case 'V':
			print_version();
			goto out_success;
		default:
			goto fail_arg;
		}
	}

	if (optind >= argc) {
		fputs("Missing argument: squashfs image\n", stderr);
		goto fail_arg;
	}

	filename = argv[optind++];

	if (optind < argc) {
		fputs("Unknown extra arguments\n", stderr);
		goto fail_arg;
	}

	if (num_subdirs > 1)
		keep_as_dir = true;

	return;
fail_errno:
	perror("parsing options");
	goto fail;
fail_arg:
	fputs("Try `sqfs2tar --help' for more information.\n", stderr);
	goto fail;
fail:
	ret = EXIT_FAILURE;
	goto out_exit;
out_success:
	ret = EXIT_SUCCESS;
	goto out_exit;
out_exit:
	for (idx = 0; idx < num_subdirs; ++idx)
		free(subdirs[idx]);
	free(subdirs);
	exit(ret);
}

static int terminate_archive(void)
{
	char buffer[1024];

	memset(buffer, '\0', sizeof(buffer));

	return write_data("adding archive terminator", STDOUT_FILENO,
			  buffer, sizeof(buffer));
}

static tar_xattr_t *gen_xattr_list(fstree_t *fs, tree_xattr_t *xattr)
{
	const char *key, *value;
	tar_xattr_t *list;
	size_t i;

	list = alloc_array(sizeof(list[0]), xattr->num_attr);
	if (list == NULL) {
		perror("creating xattr list");
		return NULL;
	}

	for (i = 0; i < xattr->num_attr; ++i) {
		key = str_table_get_string(&fs->xattr_keys,
					   xattr->attr[i].key_index);
		value = str_table_get_string(&fs->xattr_values,
					     xattr->attr[i].value_index);

		list[i].key = (char *)key;
		list[i].value = (char *)value;

		if (i + 1 < xattr->num_attr) {
			list[i].next = list + i + 1;
		} else {
			list[i].next = NULL;
		}
	}

	return list;
}

static int write_tree_dfs(fstree_t *fs, tree_node_t *n, data_reader_t *data)
{
	tar_xattr_t *xattr = NULL;
	size_t len, name_len;
	char *name, *target;
	struct stat sb;
	int ret;

	if (n->parent == NULL && S_ISDIR(n->mode))
		goto skip_hdr;

	name = fstree_get_path(n);
	if (name == NULL) {
		perror("resolving tree node path");
		return -1;
	}

	ret = canonicalize_name(name);
	assert(ret == 0);

	if (current_subdir != NULL && !keep_as_dir) {
		if (strcmp(name, current_subdir) == 0) {
			free(name);
			goto skip_hdr;
		}

		len = strlen(current_subdir);
		name_len = strlen(name);

		assert(name_len > len);
		assert(name[len] == '/');

		memmove(name, name + len + 1, name_len - len);
	}

	fstree_node_stat(fs, n, &sb);

	if (!no_xattr && n->xattr != NULL) {
		xattr = gen_xattr_list(fs, n->xattr);
		if (xattr == NULL) {
			free(name);
			return -1;
		}
	}

	target = S_ISLNK(sb.st_mode) ? n->data.slink_target : NULL;
	ret = write_tar_header(STDOUT_FILENO, &sb, name, target, xattr,
			       record_counter++);
	free(xattr);

	if (ret > 0) {
		if (dont_skip) {
			fputs("Not allowed to skip files, aborting!\n",
			      stderr);
			ret = -1;
		} else {
			fprintf(stderr, "Skipping %s\n", name);
			ret = 0;
		}
		free(name);
		return ret;
	}

	free(name);

	if (ret < 0)
		return -1;

	if (S_ISREG(n->mode)) {
		if (data_reader_dump_file(data, n->data.file,
					  STDOUT_FILENO, false)) {
			return -1;
		}

		if (padd_file(STDOUT_FILENO, n->data.file->size, 512))
			return -1;
	}
skip_hdr:
	if (S_ISDIR(n->mode)) {
		for (n = n->data.dir->children; n != NULL; n = n->next) {
			if (write_tree_dfs(fs, n, data))
				return -1;
		}
	}

	return 0;
}

int main(int argc, char **argv)
{
	int rdtree_flags = 0, status = EXIT_FAILURE;
	sqfs_compressor_config_t cfg;
	sqfs_compressor_t *cmp;
	data_reader_t *data;
	sqfs_file_t *file;
	sqfs_super_t super;
	tree_node_t *root;
	fstree_t fs;
	size_t i;

	process_args(argc, argv);

	if (!no_xattr)
		rdtree_flags |= RDTREE_READ_XATTR;

	file = sqfs_open_file(filename, SQFS_FILE_OPEN_READ_ONLY);
	if (file == NULL) {
		perror(filename);
		goto out_dirs;
	}

	if (sqfs_super_read(&super, file)) {
		fprintf(stderr, "%s: error reading super block.\n", filename);
		goto out_fd;
	}

	if (!sqfs_compressor_exists(super.compression_id)) {
		fprintf(stderr, "%s: unknown compressor used.\n", filename);
		goto out_fd;
	}

	sqfs_compressor_config_init(&cfg, super.compression_id,
				    super.block_size,
				    SQFS_COMP_FLAG_UNCOMPRESS);

	cmp = sqfs_compressor_create(&cfg);
	if (cmp == NULL)
		goto out_fd;

	if (super.flags & SQFS_FLAG_COMPRESSOR_OPTIONS) {
		if (cmp->read_options(cmp, file))
			goto out_cmp;
	}

	if (deserialize_fstree(&fs, &super, cmp, file, rdtree_flags))
		goto out_cmp;

	fstree_gen_file_list(&fs);

	data = data_reader_create(file, &super, cmp);
	if (data == NULL)
		goto out;

	for (i = 0; i < num_subdirs; ++i) {
		root = fstree_node_from_path(&fs, subdirs[i]);
		if (root == NULL) {
			perror(subdirs[i]);
			goto out;
		}

		if (!S_ISDIR(root->mode)) {
			fprintf(stderr, "%s is not a directory\n", subdirs[i]);
			goto out;
		}

		current_subdir = subdirs[i];

		if (write_tree_dfs(&fs, root, data))
			goto out;
	}

	current_subdir = NULL;

	if (num_subdirs == 0) {
		if (write_tree_dfs(&fs, fs.root, data))
			goto out;
	}

	if (terminate_archive())
		goto out;

	status = EXIT_SUCCESS;
out:
	fstree_cleanup(&fs);
out_cmp:
	cmp->destroy(cmp);
out_fd:
	file->destroy(file);
out_dirs:
	for (i = 0; i < num_subdirs; ++i)
		free(subdirs[i]);
	free(subdirs);
	return status;
}
