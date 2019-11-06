/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * tar2sqfs.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "common.h"
#include "tar.h"

#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

static struct option long_opts[] = {
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
	{ "force", no_argument, NULL, 'f' },
	{ "quiet", no_argument, NULL, 'q' },
	{ "help", no_argument, NULL, 'h' },
	{ "version", no_argument, NULL, 'V' },
};

static const char *short_opts = "c:b:B:d:X:j:Q:sxekfqhV";

static const char *usagestr =
"Usage: tar2sqfs [OPTIONS...] <sqfsfile>\n"
"\n"
"Read an uncompressed tar archive from stdin and turn it into a squashfs\n"
"filesystem image.\n"
"\n"
"Possible options:\n"
"\n"
"  --compressor, -c <name>     Select the compressor to use.\n"
"                              A list of available compressors is below.\n"
"  --comp-extra, -X <options>  A comma seperated list of extra options for\n"
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
"  --defaults, -d <options>    A comma seperated list of default values for\n"
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
static sqfs_writer_cfg_t cfg;
static sqfs_writer_t sqfs;

static void process_args(int argc, char **argv)
{
	bool have_compressor;
	int i;

	sqfs_writer_cfg_init(&cfg);

	for (;;) {
		i = getopt_long(argc, argv, short_opts, long_opts, NULL);
		if (i == -1)
			break;

		switch (i) {
		case 'b':
			cfg.block_size = strtol(optarg, NULL, 0);
			break;
		case 'B':
			cfg.devblksize = strtol(optarg, NULL, 0);
			if (cfg.devblksize < 1024) {
				fputs("Device block size must be at "
				      "least 1024\n", stderr);
				exit(EXIT_FAILURE);
			}
			break;
		case 'c':
			have_compressor = true;

			if (sqfs_compressor_id_from_name(optarg, &cfg.comp_id))
				have_compressor = false;

			if (!sqfs_compressor_exists(cfg.comp_id))
				have_compressor = false;

			if (!have_compressor) {
				fprintf(stderr, "Unsupported compressor '%s'\n",
					optarg);
				exit(EXIT_FAILURE);
			}
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
		fputs("Unknown extra arguments\n", stderr);
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
	const sparse_map_t *it;
	sqfs_inode_generic_t *inode;
	size_t max_blk_count;
	sqfs_file_t *file;
	sqfs_u64 sum;
	int ret;

	max_blk_count = filesize / cfg.block_size;
	if (filesize % cfg.block_size)
		++max_blk_count;

	inode = alloc_flex(sizeof(*inode), sizeof(sqfs_u32), max_blk_count);
	if (inode == NULL) {
		perror("creating file inode");
		return -1;
	}

	inode->block_sizes = (sqfs_u32 *)inode->extra;
	inode->base.type = SQFS_INODE_FILE;
	sqfs_inode_set_file_size(inode, filesize);
	sqfs_inode_set_frag_location(inode, 0xFFFFFFFF, 0xFFFFFFFF);

	fi->user_ptr = inode;

	if (hdr->sparse != NULL) {
		for (sum = 0, it = hdr->sparse; it != NULL; it = it->next)
			sum += it->count;

		file = sqfs_get_stdin_file(hdr->sparse, sum);
		if (file == NULL) {
			perror("packing files");
			return -1;
		}
	} else {
		file = sqfs_get_stdin_file(NULL, filesize);
		if (file == NULL) {
			perror("packing files");
			return -1;
		}
	}

	ret = write_data_from_file(hdr->name, sqfs.data, inode, file, 0);
	file->destroy(file);

	sqfs.stats.bytes_read += filesize;
	sqfs.stats.file_count += 1;

	if (ret)
		return -1;

	return skip_padding(stdin, hdr->sparse == NULL ?
			    filesize : hdr->record_size);
}

static int copy_xattr(tree_node_t *node, tar_header_decoded_t *hdr)
{
	tar_xattr_t *xattr;
	int ret;

	ret = sqfs_xattr_writer_begin(sqfs.xwr);
	if (ret) {
		sqfs_perror(hdr->name, "beginning xattr block", ret);
		return -1;
	}

	for (xattr = hdr->xattr; xattr != NULL; xattr = xattr->next) {
		if (!sqfs_has_xattr(xattr->key)) {
			fprintf(stderr, "%s: squashfs does not "
				"support xattr prefix of %s\n",
				dont_skip ? "ERROR" : "WARNING",
				xattr->key);

			if (dont_skip)
				return -1;
			continue;
		}

		ret = sqfs_xattr_writer_add(sqfs.xwr, xattr->key, xattr->value,
					    strlen(xattr->value));
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

static int process_tar_ball(void)
{
	tar_header_decoded_t hdr;
	sqfs_u64 offset, count;
	sparse_map_t *m;
	bool skip;
	int ret;

	for (;;) {
		ret = read_header(stdin, &hdr);
		if (ret > 0)
			break;
		if (ret < 0)
			return -1;

		skip = false;

		if (hdr.name != NULL && strcmp(hdr.name, "./") == 0 &&
		    S_ISDIR(hdr.sb.st_mode)) {
			/* XXX: tar entries might be prefixed with ./ which is
			   stripped by cannonicalize_name, but the tar file may
			   contain a directory entry named './' */
			clear_header(&hdr);
			continue;
		}

		if (hdr.name == NULL || canonicalize_name(hdr.name) != 0) {
			fprintf(stderr, "skipping '%s' (invalid name)\n",
				hdr.name);
			skip = true;
		}

		if (hdr.name[0] == '\0') {
			fputs("skipping entry with empty name\n", stderr);
			skip = true;
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
			if (skip_entry(stdin, hdr.sb.st_size))
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

	if (sqfs_writer_init(&sqfs, &cfg))
		return EXIT_FAILURE;

	if (process_tar_ball())
		goto out;

	if (sqfs_writer_finish(&sqfs, &cfg))
		goto out;

	status = EXIT_SUCCESS;
out:
	sqfs_writer_cleanup(&sqfs);
	return status;
}
