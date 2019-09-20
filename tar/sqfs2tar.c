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

static char **subdirs = NULL;
static size_t num_subdirs = 0;
static size_t max_subdirs = 0;

static sqfs_xattr_reader_t *xr;
static data_reader_t *data;
static sqfs_file_t *file;
static sqfs_super_t super;

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

static int get_xattrs(const sqfs_inode_generic_t *inode, tar_xattr_t **out)
{
	tar_xattr_t *list = NULL, *ent;
	sqfs_xattr_value_t *value;
	sqfs_xattr_entry_t *key;
	sqfs_xattr_id_t desc;
	uint32_t index;
	size_t i;

	if (xr == NULL)
		return 0;

	switch (inode->base.type) {
	case SQFS_INODE_EXT_DIR:
		index = inode->data.dir_ext.xattr_idx;
		break;
	case SQFS_INODE_EXT_FILE:
		index = inode->data.file_ext.xattr_idx;
		break;
	case SQFS_INODE_EXT_SLINK:
		index = inode->data.slink_ext.xattr_idx;
		break;
	case SQFS_INODE_EXT_BDEV:
	case SQFS_INODE_EXT_CDEV:
		index = inode->data.dev_ext.xattr_idx;
		break;
	case SQFS_INODE_EXT_FIFO:
	case SQFS_INODE_EXT_SOCKET:
		index = inode->data.ipc_ext.xattr_idx;
		break;
	default:
		return 0;
	}

	if (index == 0xFFFFFFFF)
		return 0;

	if (sqfs_xattr_reader_get_desc(xr, index, &desc)) {
		fputs("Error resolving xattr index\n", stderr);
		return -1;
	}

	if (sqfs_xattr_reader_seek_kv(xr, &desc)) {
		fputs("Error locating xattr key-value pairs\n", stderr);
		return -1;
	}

	for (i = 0; i < desc.count; ++i) {
		if (sqfs_xattr_reader_read_key(xr, &key)) {
			fputs("Error reading xattr key\n", stderr);
			return -1;
		}

		if (sqfs_xattr_reader_read_value(xr, key, &value)) {
			fputs("Error reading xattr value\n", stderr);
			free(key);
			return -1;
		}

		ent = calloc(1, sizeof(*ent) + strlen((const char *)key->key) +
			     value->size + 2);
		if (ent == NULL) {
			perror("creating xattr entry");
			goto fail;
		}

		ent->key = ent->data;
		strcpy(ent->key, (const char *)key->key);

		ent->value = ent->key + strlen(ent->key) + 1;
		memcpy(ent->value, value->value, value->size + 1);

		ent->next = list;
		list = ent;

		free(key);
		free(value);
	}

	*out = list;
	return 0;
fail:
	while (list != NULL) {
		ent = list;
		list = list->next;
		free(ent);
	}
	return -1;
}

static int write_tree_dfs(const sqfs_tree_node_t *n)
{
	tar_xattr_t *xattr = NULL, *xit;
	char *name, *target;
	struct stat sb;
	int ret;

	if (n->parent == NULL && S_ISDIR(n->inode->base.mode))
		goto skip_hdr;

	name = sqfs_tree_node_get_path(n);
	if (name == NULL) {
		perror("resolving tree node path");
		return -1;
	}

	if (canonicalize_name(name))
		goto out_skip;

	inode_stat(n, &sb);

	if (!no_xattr) {
		if (get_xattrs(n->inode, &xattr))
			return -1;
	}

	target = S_ISLNK(sb.st_mode) ? n->inode->slink_target : NULL;
	ret = write_tar_header(STDOUT_FILENO, &sb, name, target, xattr,
			       record_counter++);

	while (xattr != NULL) {
		xit = xattr;
		xattr = xattr->next;
		free(xit);
	}

	if (ret > 0)
		goto out_skip;

	free(name);
	if (ret < 0)
		return -1;

	if (S_ISREG(sb.st_mode)) {
		if (data_reader_dump(data, n->inode, STDOUT_FILENO,
				     super.block_size, false))
			return -1;

		if (padd_file(STDOUT_FILENO, sb.st_size, 512))
			return -1;
	}
skip_hdr:
	for (n = n->children; n != NULL; n = n->next) {
		if (write_tree_dfs(n))
			return -1;
	}
	return 0;
out_skip:
	if (dont_skip) {
		fputs("Not allowed to skip files, aborting!\n", stderr);
		ret = -1;
	} else {
		fprintf(stderr, "Skipping %s\n", name);
		ret = 0;
	}
	free(name);
	return ret;
}

static sqfs_tree_node_t *tree_merge(sqfs_tree_node_t *lhs,
				    sqfs_tree_node_t *rhs)
{
	sqfs_tree_node_t *head = NULL, **next_ptr = &head;
	sqfs_tree_node_t *it, *l, *r;
	int diff;

	while (lhs->children != NULL && rhs->children != NULL) {
		diff = strcmp((const char *)lhs->children->name,
			      (const char *)rhs->children->name);

		if (diff < 0) {
			it = lhs->children;
			lhs->children = lhs->children->next;
		} else if (diff > 0) {
			it = rhs->children;
			rhs->children = rhs->children->next;
		} else {
			l = lhs->children;
			lhs->children = lhs->children->next;

			r = rhs->children;
			rhs->children = rhs->children->next;

			it = tree_merge(l, r);
		}

		*next_ptr = it;
		next_ptr = &it->next;
	}

	it = (lhs->children != NULL ? lhs->children : rhs->children);
	*next_ptr = it;

	sqfs_dir_tree_destroy(rhs);
	lhs->children = head;
	return lhs;
}

int main(int argc, char **argv)
{
	sqfs_tree_node_t *root = NULL, *subtree;
	int flags, ret, status = EXIT_FAILURE;
	sqfs_compressor_config_t cfg;
	sqfs_compressor_t *cmp;
	sqfs_id_table_t *idtbl;
	sqfs_dir_reader_t *dr;
	size_t i;

	process_args(argc, argv);

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

	idtbl = sqfs_id_table_create();

	if (idtbl == NULL) {
		perror("creating ID table");
		goto out_cmp;
	}

	if (sqfs_id_table_read(idtbl, file, &super, cmp)) {
		fputs("error loading ID table\n", stderr);
		goto out_id;
	}

	data = data_reader_create(file, super.block_size, cmp);
	if (data == NULL)
		goto out_id;

	if (data_reader_load_fragment_table(data, &super))
		goto out_data;

	dr = sqfs_dir_reader_create(&super, cmp, file);
	if (dr == NULL) {
		perror("creating dir reader");
		goto out_data;
	}

	if (!no_xattr && (super.flags & SQFS_FLAG_NO_XATTRS)) {
		xr = sqfs_xattr_reader_create(file, &super, cmp);
		if (xr == NULL) {
			goto out_dr;
		}

		if (sqfs_xattr_reader_load_locations(xr)) {
			fputs("error loading xattr table\n", stderr);
			goto out_xr;
		}
	}

	if (num_subdirs == 0) {
		ret = sqfs_dir_reader_get_full_hierarchy(dr, idtbl, NULL,
							 0, &root);
		if (ret) {
			fputs("error loading file system tree", stderr);
			goto out;
		}
	} else {
		flags = 0;

		if (keep_as_dir || num_subdirs > 1)
			flags = SQFS_TREE_STORE_PARENTS;

		for (i = 0; i < num_subdirs; ++i) {
			ret = sqfs_dir_reader_get_full_hierarchy(dr, idtbl,
								 subdirs[i],
								 flags,
								 &subtree);
			if (ret) {
				fprintf(stderr, "error loading '%s'\n",
					subdirs[i]);
				goto out;
			}

			if (root == NULL) {
				root = subtree;
			} else {
				root = tree_merge(root, subtree);
			}
		}
	}

	if (write_tree_dfs(root))
		goto out;

	if (terminate_archive())
		goto out;

	status = EXIT_SUCCESS;
out:
	if (root != NULL)
		sqfs_dir_tree_destroy(root);
out_xr:
	if (xr != NULL)
		sqfs_xattr_reader_destroy(xr);
out_dr:
	sqfs_dir_reader_destroy(dr);
out_data:
	data_reader_destroy(data);
out_id:
	sqfs_id_table_destroy(idtbl);
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
