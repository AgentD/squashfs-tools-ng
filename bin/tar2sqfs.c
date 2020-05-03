/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * tar2sqfs.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "common.h"
#include "compat.h"
#include "tar.h"

#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>

#ifdef _WIN32
#include <io.h>
#endif

static struct option long_opts[] = {
	{ "root-becomes", required_argument, NULL, 'r' },
	{ "compressor", required_argument, NULL, 'c' },
	{ "block-size", required_argument, NULL, 'b' },
	{ "dev-block-size", required_argument, NULL, 'B' },
	{ "defaults", required_argument, NULL, 'd' },
	{ "num-jobs", required_argument, NULL, 'j' },
	{ "queue-backlog", required_argument, NULL, 'Q' },
	{ "comp-extra", required_argument, NULL, 'X' },
	{ "no-skip", no_argument, NULL, 's' },
	{ "no-xattr", no_argument, NULL, 'x' },
	{ "no-keep-time", no_argument, NULL, 'k' },
	{ "exportable", no_argument, NULL, 'e' },
	{ "no-tail-packing", no_argument, NULL, 'T' },
	{ "force", no_argument, NULL, 'f' },
	{ "quiet", no_argument, NULL, 'q' },
	{ "help", no_argument, NULL, 'h' },
	{ "version", no_argument, NULL, 'V' },
	{ NULL, 0, NULL, 0 },
};

static const char *short_opts = "r:c:b:B:d:X:j:Q:sxekfqThV";

static const char *usagestr =
"Usage: tar2sqfs [OPTIONS...] <sqfsfile>\n"
"\n"
"Read an uncompressed tar archive from stdin and turn it into a squashfs\n"
"filesystem image.\n"
"\n"
"Possible options:\n"
"\n"
"  --root-becomes, -r <dir>    The specified directory becomes the root.\n"
"                              Only its children are packed into the image\n"
"                              and its attributes (ownership, permissions,\n"
"                              xattrs, ...) are stored in the root inode.\n"
"                              If not set and a tarbal has an entry for './'\n"
"                              or '/', it becomes the root instead.\n"
"\n"
"  --compressor, -c <name>     Select the compressor to use.\n"
"                              A list of available compressors is below.\n"
"  --comp-extra, -X <options>  A comma separated list of extra options for\n"
"                              the selected compressor. Specify 'help' to\n"
"                              get a list of available options.\n"
"  --num-jobs, -j <count>      Number of compressor jobs to create.\n"
"  --queue-backlog, -Q <count> Maximum number of data blocks in the thread\n"
"                              worker queue before the packer starts waiting\n"
"                              for the block processors to catch up.\n"
"                              Defaults to 10 times the number of jobs.\n"
"  --block-size, -b <size>     Block size to use for Squashfs image.\n"
"                              Defaults to %u.\n"
"  --dev-block-size, -B <size> Device block size to padd the image to.\n"
"                              Defaults to %u.\n"
"  --defaults, -d <options>    A comma separated list of default values for\n"
"                              implicitly created directories.\n"
"\n"
"                              Possible options:\n"
"                                 uid=<value>    0 if not set.\n"
"                                 gid=<value>    0 if not set.\n"
"                                 mode=<value>   0755 if not set.\n"
"                                 mtime=<value>  0 if not set.\n"
"\n"
"  --no-skip, -s               Abort if a tar record cannot be read instead\n"
"                              of skipping it.\n"
"  --no-xattr, -x              Do not copy extended attributes from archive.\n"
"  --no-keep-time, -k          Do not keep the time stamps stored in the\n"
"                              archive. Instead, set defaults on all files.\n"
"  --exportable, -e            Generate an export table for NFS support.\n"
"  --no-tail-packing, -T       Do not perform tail end packing on files that\n"
"                              are larger than block size.\n"
"  --force, -f                 Overwrite the output file if it exists.\n"
"  --quiet, -q                 Do not print out progress reports.\n"
"  --help, -h                  Print help text and exit.\n"
"  --version, -V               Print version information and exit.\n"
"\n"
"Examples:\n"
"\n"
"\ttar2sqfs rootfs.sqfs < rootfs.tar\n"
"\tzcat rootfs.tar.gz | tar2sqfs rootfs.sqfs\n"
"\txzcat rootfs.tar.xz | tar2sqfs rootfs.sqfs\n"
"\n";

static bool dont_skip = false;
static bool keep_time = true;
static bool no_tail_pack = false;
static sqfs_writer_cfg_t cfg;
static sqfs_writer_t sqfs;
static FILE *input_file = NULL;
static char *root_becomes = NULL;

static void process_args(int argc, char **argv)
{
	bool have_compressor;
	int i, ret;

	sqfs_writer_cfg_init(&cfg);

	for (;;) {
		i = getopt_long(argc, argv, short_opts, long_opts, NULL);
		if (i == -1)
			break;

		switch (i) {
		case 'T':
			no_tail_pack = true;
			break;
		case 'b':
			if (parse_size("Block size", &cfg.block_size,
				       optarg, 0)) {
				exit(EXIT_FAILURE);
			}
			break;
		case 'B':
			if (parse_size("Device block size", &cfg.devblksize,
				       optarg, 0)) {
				exit(EXIT_FAILURE);
			}
			if (cfg.devblksize < 1024) {
				fputs("Device block size must be at "
				      "least 1024\n", stderr);
				exit(EXIT_FAILURE);
			}
			break;
		case 'c':
			have_compressor = true;
			ret = sqfs_compressor_id_from_name(optarg);

			if (ret < 0) {
				have_compressor = false;
#ifdef WITH_LZO
				if (cfg.comp_id == SQFS_COMP_LZO)
					have_compressor = true;
#endif
			}

			if (!have_compressor) {
				fprintf(stderr, "Unsupported compressor '%s'\n",
					optarg);
				exit(EXIT_FAILURE);
			}

			cfg.comp_id = ret;
			break;
		case 'j':
			cfg.num_jobs = strtol(optarg, NULL, 0);
			break;
		case 'Q':
			cfg.max_backlog = strtol(optarg, NULL, 0);
			break;
		case 'X':
			cfg.comp_extra = optarg;
			break;
		case 'd':
			cfg.fs_defaults = optarg;
			break;
		case 'x':
			cfg.no_xattr = true;
			break;
		case 'k':
			keep_time = false;
			break;
		case 'r':
			free(root_becomes);
			root_becomes = strdup(optarg);
			if (root_becomes == NULL) {
				perror("copying root directory name");
				exit(EXIT_FAILURE);
			}

			if (canonicalize_name(root_becomes) != 0 ||
			    strlen(root_becomes) == 0) {
				fprintf(stderr,
					"Invalid root directory '%s'.\n",
					optarg);
				goto fail_arg;
			}
			break;
		case 's':
			dont_skip = true;
			break;
		case 'e':
			cfg.exportable = true;
			break;
		case 'f':
			cfg.outmode |= SQFS_FILE_OPEN_OVERWRITE;
			break;
		case 'q':
			cfg.quiet = true;
			break;
		case 'h':
			printf(usagestr, SQFS_DEFAULT_BLOCK_SIZE,
			       SQFS_DEVBLK_SIZE);
			compressor_print_available();
			exit(EXIT_SUCCESS);
		case 'V':
			print_version("tar2sqfs");
			exit(EXIT_SUCCESS);
		default:
			goto fail_arg;
		}
	}

	if (cfg.num_jobs < 1)
		cfg.num_jobs = 1;

	if (cfg.max_backlog < 1)
		cfg.max_backlog = 10 * cfg.num_jobs;

	if (cfg.comp_extra != NULL && strcmp(cfg.comp_extra, "help") == 0) {
		compressor_print_help(cfg.comp_id);
		exit(EXIT_SUCCESS);
	}

	if (optind >= argc) {
		fputs("Missing argument: squashfs image\n", stderr);
		goto fail_arg;
	}

	cfg.filename = argv[optind++];

	if (optind < argc) {
		fputs("Unknown extra arguments specified.\n", stderr);
		goto fail_arg;
	}
	return;
fail_arg:
	fputs("Try `tar2sqfs --help' for more information.\n", stderr);
	exit(EXIT_FAILURE);
}

static int write_file(tar_header_decoded_t *hdr, file_info_t *fi,
		      sqfs_u64 filesize)
{
	sqfs_file_t *file;
	int flags;
	int ret;

	file = sqfs_get_stdin_file(input_file, hdr->sparse, filesize);
	if (file == NULL) {
		perror("packing files");
		return -1;
	}

	flags = 0;
	if (no_tail_pack && filesize > cfg.block_size)
		flags |= SQFS_BLK_DONT_FRAGMENT;

	ret = write_data_from_file(hdr->name, sqfs.data,
				   (sqfs_inode_generic_t **)&fi->user_ptr,
				   file, flags);
	sqfs_destroy(file);

	if (ret)
		return -1;

	return skip_padding(input_file, hdr->sparse == NULL ?
			    filesize : hdr->record_size);
}

static int copy_xattr(tree_node_t *node, const tar_header_decoded_t *hdr)
{
	tar_xattr_t *xattr;
	int ret;

	ret = sqfs_xattr_writer_begin(sqfs.xwr);
	if (ret) {
		sqfs_perror(hdr->name, "beginning xattr block", ret);
		return -1;
	}

	for (xattr = hdr->xattr; xattr != NULL; xattr = xattr->next) {
		if (sqfs_get_xattr_prefix_id(xattr->key) < 0) {
			fprintf(stderr, "%s: squashfs does not "
				"support xattr prefix of %s\n",
				dont_skip ? "ERROR" : "WARNING",
				xattr->key);

			if (dont_skip)
				return -1;
			continue;
		}

		ret = sqfs_xattr_writer_add(sqfs.xwr, xattr->key, xattr->value,
					    xattr->value_len);
		if (ret) {
			sqfs_perror(hdr->name, "storing xattr key-value pair",
				    ret);
			return -1;
		}
	}

	ret = sqfs_xattr_writer_end(sqfs.xwr, &node->xattr_idx);
	if (ret) {
		sqfs_perror(hdr->name, "completing xattr block", ret);
		return -1;
	}

	return 0;
}

static int create_node_and_repack_data(tar_header_decoded_t *hdr)
{
	tree_node_t *node;

	if (hdr->is_hard_link) {
		node = fstree_add_hard_link(&sqfs.fs, hdr->name,
					    hdr->link_target);
		if (node == NULL)
			goto fail_errno;

		if (!cfg.quiet) {
			printf("Hard link %s -> %s\n", hdr->name,
			       hdr->link_target);
		}
		return 0;
	}

	if (!keep_time) {
		hdr->sb.st_mtime = sqfs.fs.defaults.st_mtime;
	}

	node = fstree_add_generic(&sqfs.fs, hdr->name,
				  &hdr->sb, hdr->link_target);
	if (node == NULL)
		goto fail_errno;

	if (!cfg.quiet)
		printf("Packing %s\n", hdr->name);

	if (!cfg.no_xattr) {
		if (copy_xattr(node, hdr))
			return -1;
	}

	if (S_ISREG(hdr->sb.st_mode)) {
		if (write_file(hdr, &node->data.file, hdr->sb.st_size))
			return -1;
	}

	return 0;
fail_errno:
	perror(hdr->name);
	return -1;
}

static int set_root_attribs(const tar_header_decoded_t *hdr)
{
	if (hdr->is_hard_link || !S_ISDIR(hdr->sb.st_mode)) {
		fprintf(stderr, "'%s' is not a directory!\n", hdr->name);
		return -1;
	}

	sqfs.fs.root->uid = hdr->sb.st_uid;
	sqfs.fs.root->gid = hdr->sb.st_gid;
	sqfs.fs.root->mode = hdr->sb.st_mode;

	if (keep_time)
		sqfs.fs.root->mod_time = hdr->sb.st_mtime;

	if (!cfg.no_xattr) {
		if (copy_xattr(sqfs.fs.root, hdr))
			return -1;
	}

	return 0;
}

static int process_tar_ball(void)
{
	bool skip, is_root, is_prefixed;
	tar_header_decoded_t hdr;
	sqfs_u64 offset, count;
	sparse_map_t *m;
	size_t rootlen;
	int ret;

	rootlen = root_becomes == NULL ? 0 : strlen(root_becomes);

	for (;;) {
		ret = read_header(input_file, &hdr);
		if (ret > 0)
			break;
		if (ret < 0)
			return -1;

		if (hdr.mtime < 0)
			hdr.mtime = 0;

		if ((sqfs_u64)hdr.mtime > 0x0FFFFFFFFUL)
			hdr.mtime = 0x0FFFFFFFFUL;

		hdr.sb.st_mtime = hdr.mtime;

		skip = false;
		is_root = false;
		is_prefixed = true;

		if (hdr.name == NULL || canonicalize_name(hdr.name) != 0) {
			fprintf(stderr, "skipping '%s' (invalid name)\n",
				hdr.name);
			skip = true;
		}

		if (root_becomes != NULL) {
			if (strncmp(hdr.name, root_becomes, rootlen) == 0) {
				if (hdr.name[rootlen] == '\0') {
					is_root = true;
				} else if (hdr.name[rootlen] != '/') {
					is_prefixed = false;
				}
			} else {
				is_prefixed = false;
			}

			if (is_prefixed && !is_root) {
				memmove(hdr.name, hdr.name + rootlen + 1,
					strlen(hdr.name + rootlen + 1) + 1);
			}

			if (is_prefixed && hdr.name[0] == '\0') {
				fputs("skipping entry with empty name\n",
				      stderr);
				skip = true;
			}
		} else if (hdr.name[0] == '\0') {
			is_root = true;
		}

		if (!is_prefixed) {
			clear_header(&hdr);
			continue;
		}

		if (is_root) {
			if (set_root_attribs(&hdr))
				goto fail;
			clear_header(&hdr);
			continue;
		}

		if (!skip && hdr.unknown_record) {
			fprintf(stderr, "%s: unknown entry type\n", hdr.name);
			skip = true;
		}

		if (!skip && hdr.sparse != NULL) {
			offset = hdr.sparse->offset;
			count = 0;

			for (m = hdr.sparse; m != NULL; m = m->next) {
				if (m->offset < offset) {
					skip = true;
					break;
				}
				offset = m->offset + m->count;
				count += m->count;
			}

			if (count != hdr.record_size)
				skip = true;

			if (skip) {
				fprintf(stderr, "%s: broken sparse "
					"file layout\n", hdr.name);
			}
		}

		if (skip) {
			if (dont_skip)
				goto fail;
			if (skip_entry(input_file, hdr.sb.st_size))
				goto fail;

			clear_header(&hdr);
			continue;
		}

		if (create_node_and_repack_data(&hdr))
			goto fail;

		clear_header(&hdr);
	}

	return 0;
fail:
	clear_header(&hdr);
	return -1;
}

int main(int argc, char **argv)
{
	int status = EXIT_FAILURE;

	process_args(argc, argv);

#ifdef _WIN32
	_setmode(_fileno(stdin), _O_BINARY);
	input_file = stdin;
#else
	input_file = freopen(NULL, "rb", stdin);
#endif

	if (input_file == NULL) {
		perror("changing stdin to binary mode");
		return EXIT_FAILURE;
	}

	if (sqfs_writer_init(&sqfs, &cfg))
		return EXIT_FAILURE;

	if (process_tar_ball())
		goto out;

	if (fstree_post_process(&sqfs.fs))
		goto out;

	if (sqfs_writer_finish(&sqfs, &cfg))
		goto out;

	status = EXIT_SUCCESS;
out:
	sqfs_writer_cleanup(&sqfs, status);
	return status;
}
