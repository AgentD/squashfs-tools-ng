/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * sqfs2tar.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "common.h"
#include "tar.h"

#include <getopt.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>

static struct option long_opts[] = {
	{ "subdir", required_argument, NULL, 'd' },
	{ "keep-as-dir", no_argument, NULL, 'k' },
	{ "root-becomes", required_argument, NULL, 'r' },
	{ "no-skip", no_argument, NULL, 's' },
	{ "no-xattr", no_argument, NULL, 'X' },
	{ "no-hard-links", no_argument, NULL, 'L' },
	{ "help", no_argument, NULL, 'h' },
	{ "version", no_argument, NULL, 'V' },
	{ NULL, 0, NULL, 0 },
};

static const char *short_opts = "d:kr:sXLhV";

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
"  --root-becomes, -r <dir>  Turn the root inode into a directory with the\n"
"                            specified name. Everything else will be stored\n"
"                            inside this directory. The special value '.' is\n"
"                            allowed to prefix all tar paths with './' and\n"
"                            add an entry named '.' for the root inode.\n"
"                            If this option isn't used, all meta data stored\n"
"                            in the root inode IS LOST!\n"
"\n"
"  --keep-as-dir, -k         If --subdir is used only once, don't make the\n"
"                            subdir the archive root, instead keep it as\n"
"                            prefix for all unpacked files.\n"
"                            Using --subdir more than once implies\n"
"                            --keep-as-dir.\n"
"  --no-xattr, -X            Do not copy extended attributes.\n"
"  --no-hard-links, -L       Do not generate hard links. Produce duplicate\n"
"                            entries instead.\n"
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
static bool no_links = false;

static char *root_becomes = NULL;
static char **subdirs = NULL;
static size_t num_subdirs = 0;
static size_t max_subdirs = 0;

static sqfs_xattr_reader_t *xr;
static sqfs_data_reader_t *data;
static sqfs_file_t *file;
static sqfs_super_t super;
static sqfs_hard_link_t *links = NULL;

static FILE *out_file = NULL;

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
		case 'r':
			free(root_becomes);
			root_becomes = strdup(optarg);
			if (root_becomes == NULL)
				goto fail_errno;

			if (strcmp(root_becomes, "./") == 0)
				root_becomes[1] = '\0';

			if (strcmp(root_becomes, ".") == 0)
				break;

			if (canonicalize_name(root_becomes) != 0 ||
			    strlen(root_becomes) == 0) {
				fprintf(stderr,
					"Invalid root directory '%s'.\n",
					optarg);
				goto fail_arg;
			}
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
		case 'L':
			no_links = true;
			break;
		case 'h':
			fputs(usagestr, stdout);
			goto out_success;
		case 'V':
			print_version("sqfs2tar");
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
	free(root_becomes);
	free(subdirs);
	exit(ret);
}

static int terminate_archive(void)
{
	char buffer[1024];

	memset(buffer, '\0', sizeof(buffer));

	return write_retry("adding archive terminator", out_file,
			   buffer, sizeof(buffer));
}

static int get_xattrs(const char *name, const sqfs_inode_generic_t *inode,
		      tar_xattr_t **out)
{
	tar_xattr_t *list = NULL, *ent;
	sqfs_xattr_value_t *value;
	sqfs_xattr_entry_t *key;
	sqfs_xattr_id_t desc;
	sqfs_u32 index;
	size_t i;
	int ret;

	if (xr == NULL)
		return 0;

	sqfs_inode_get_xattr_index(inode, &index);

	if (index == 0xFFFFFFFF)
		return 0;

	ret = sqfs_xattr_reader_get_desc(xr, index, &desc);
	if (ret) {
		sqfs_perror(name, "resolving xattr index", ret);
		return -1;
	}

	ret = sqfs_xattr_reader_seek_kv(xr, &desc);
	if (ret) {
		sqfs_perror(name, "locating xattr key-value pairs", ret);
		return -1;
	}

	for (i = 0; i < desc.count; ++i) {
		ret = sqfs_xattr_reader_read_key(xr, &key);
		if (ret) {
			sqfs_perror(name, "reading xattr key", ret);
			goto fail;
		}

		ret = sqfs_xattr_reader_read_value(xr, key, &value);
		if (ret) {
			sqfs_perror(name, "reading xattr value", ret);
			free(key);
			goto fail;
		}

		ent = calloc(1, sizeof(*ent) + strlen((const char *)key->key) +
			     value->size + 2);
		if (ent == NULL) {
			perror("creating xattr entry");
			free(key);
			free(value);
			goto fail;
		}

		ent->key = ent->data;
		strcpy(ent->key, (const char *)key->key);

		ent->value = (sqfs_u8 *)ent->key + strlen(ent->key) + 1;
		memcpy(ent->value, value->value, value->size + 1);

		ent->value_len = value->size;
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

static char *assemble_tar_path(char *name, bool is_dir)
{
	size_t len, new_len;
	char *temp;
	(void)is_dir;

	if (root_becomes == NULL && !is_dir)
		return name;

	new_len = strlen(name);
	if (root_becomes != NULL)
		new_len += strlen(root_becomes) + 1;
	if (is_dir)
		new_len += 1;

	temp = realloc(name, new_len + 1);
	if (temp == NULL) {
		perror("assembling tar entry filename");
		free(name);
		return NULL;
	}

	name = temp;

	if (root_becomes != NULL) {
		len = strlen(root_becomes);

		memmove(name + len + 1, name, strlen(name) + 1);
		memcpy(name, root_becomes, len);
		name[len] = '/';
	}

	if (is_dir) {
		len = strlen(name);

		if (len == 0 || name[len - 1] != '/') {
			name[len++] = '/';
			name[len] = '\0';
		}
	}

	return name;
}

static int write_tree_dfs(const sqfs_tree_node_t *n)
{
	tar_xattr_t *xattr = NULL, *xit;
	sqfs_hard_link_t *lnk = NULL;
	char *name, *target;
	struct stat sb;
	size_t len;
	int ret;

	inode_stat(n, &sb);

	if (n->parent == NULL) {
		if (root_becomes == NULL)
			goto skip_hdr;

		len = strlen(root_becomes);
		name = malloc(len + 2);
		if (name == NULL) {
			perror("creating root directory");
			return -1;
		}

		memcpy(name, root_becomes, len);
		name[len] = '/';
		name[len + 1] = '\0';
	} else {
		if (!is_filename_sane((const char *)n->name, false)) {
			fprintf(stderr, "Found a file named '%s', skipping.\n",
				n->name);
			if (dont_skip) {
				fputs("Not allowed to skip files, aborting!\n",
				      stderr);
				return -1;
			}
			return 0;
		}

		name = sqfs_tree_node_get_path(n);
		if (name == NULL) {
			perror("resolving tree node path");
			return -1;
		}

		if (canonicalize_name(name))
			goto out_skip;

		for (lnk = links; lnk != NULL; lnk = lnk->next) {
			if (lnk->inode_number == n->inode->base.inode_number) {
				if (strcmp(name, lnk->target) == 0)
					lnk = NULL;
				break;
			}
		}

		name = assemble_tar_path(name, S_ISDIR(sb.st_mode));
		if (name == NULL)
			return -1;
	}

	if (lnk != NULL) {
		ret = write_hard_link(out_file, &sb, name, lnk->target,
				      record_counter++);
		free(name);
		return ret;
	}

	if (!no_xattr) {
		if (get_xattrs(name, n->inode, &xattr)) {
			free(name);
			return -1;
		}
	}

	target = S_ISLNK(sb.st_mode) ? (char *)n->inode->extra : NULL;
	ret = write_tar_header(out_file, &sb, name, target, xattr,
			       record_counter++);

	while (xattr != NULL) {
		xit = xattr;
		xattr = xattr->next;
		free(xit);
	}

	if (ret > 0)
		goto out_skip;

	if (ret < 0) {
		free(name);
		return -1;
	}

	if (S_ISREG(sb.st_mode)) {
		if (sqfs_data_reader_dump(name, data, n->inode, out_file,
					  super.block_size, false)) {
			free(name);
			return -1;
		}

		if (padd_file(out_file, sb.st_size)) {
			free(name);
			return -1;
		}
	}

	free(name);
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
	sqfs_hard_link_t *lnk;
	size_t i;

	process_args(argc, argv);

#ifdef _WIN32
	_setmode(_fileno(stdout), _O_BINARY);
	out_file = stdout;
#else
	out_file = freopen(NULL, "wb", stdout);
#endif

	if (out_file == NULL) {
		perror("changing stdout to binary mode");
		goto out_dirs;
	}

	file = sqfs_open_file(filename, SQFS_FILE_OPEN_READ_ONLY);
	if (file == NULL) {
		perror(filename);
		goto out_dirs;
	}

	ret = sqfs_super_read(&super, file);
	if (ret) {
		sqfs_perror(filename, "reading super block", ret);
		goto out_fd;
	}

	sqfs_compressor_config_init(&cfg, super.compression_id,
				    super.block_size,
				    SQFS_COMP_FLAG_UNCOMPRESS);

	ret = sqfs_compressor_create(&cfg, &cmp);

#ifdef WITH_LZO
	if (super.compression_id == SQFS_COMP_LZO && ret != 0)
		ret = lzo_compressor_create(&cfg, &cmp);
#endif

	if (ret != 0) {
		sqfs_perror(filename, "creating compressor", ret);
		goto out_fd;
	}

	idtbl = sqfs_id_table_create(0);

	if (idtbl == NULL) {
		perror("creating ID table");
		goto out_cmp;
	}

	ret = sqfs_id_table_read(idtbl, file, &super, cmp);
	if (ret) {
		sqfs_perror(filename, "loading ID table", ret);
		goto out_id;
	}

	data = sqfs_data_reader_create(file, super.block_size, cmp);
	if (data == NULL) {
		sqfs_perror(filename, "creating data reader",
			    SQFS_ERROR_ALLOC);
		goto out_id;
	}

	ret = sqfs_data_reader_load_fragment_table(data, &super);
	if (ret) {
		sqfs_perror(filename, "loading fragment table", ret);
		goto out_data;
	}

	dr = sqfs_dir_reader_create(&super, cmp, file);
	if (dr == NULL) {
		sqfs_perror(filename, "creating dir reader",
			    SQFS_ERROR_ALLOC);
		goto out_data;
	}

	if (!no_xattr && !(super.flags & SQFS_FLAG_NO_XATTRS)) {
		xr = sqfs_xattr_reader_create(0);
		if (xr == NULL) {
			sqfs_perror(filename, "creating xattr reader",
				    SQFS_ERROR_ALLOC);
			goto out_dr;
		}

		ret = sqfs_xattr_reader_load(xr, &super, file, cmp);
		if (ret) {
			sqfs_perror(filename, "loading xattr table", ret);
			goto out_xr;
		}
	}

	if (num_subdirs == 0) {
		ret = sqfs_dir_reader_get_full_hierarchy(dr, idtbl, NULL,
							 0, &root);
		if (ret) {
			sqfs_perror(filename, "loading filesystem tree", ret);
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
				sqfs_perror(subdirs[i], "loading filesystem "
					    "tree", ret);
				goto out;
			}

			if (root == NULL) {
				root = subtree;
			} else {
				root = tree_merge(root, subtree);
			}
		}
	}

	if (!no_links) {
		if (sqfs_tree_find_hard_links(root, &links))
			goto out_tree;

		for (lnk = links; lnk != NULL; lnk = lnk->next) {
			lnk->target = assemble_tar_path(lnk->target, false);
			if (lnk->target == NULL)
				goto out;
		}
	}

	if (write_tree_dfs(root))
		goto out;

	if (terminate_archive())
		goto out;

	status = EXIT_SUCCESS;
	fflush(out_file);
out:
	while (links != NULL) {
		lnk = links;
		links = links->next;
		free(lnk->target);
		free(lnk);
	}
out_tree:
	if (root != NULL)
		sqfs_dir_tree_destroy(root);
out_xr:
	if (xr != NULL)
		sqfs_destroy(xr);
out_dr:
	sqfs_destroy(dr);
out_data:
	sqfs_destroy(data);
out_id:
	sqfs_destroy(idtbl);
out_cmp:
	sqfs_destroy(cmp);
out_fd:
	sqfs_destroy(file);
out_dirs:
	for (i = 0; i < num_subdirs; ++i)
		free(subdirs[i]);
	free(subdirs);
	free(root_becomes);
	return status;
}
