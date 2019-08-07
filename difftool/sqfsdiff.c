/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * sqfsdiff.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "difftool.h"

static struct option long_opts[] = {
	{ "no-owner", no_argument, NULL, 'O' },
	{ "no-permissions", no_argument, NULL, 'P' },
	{ "no-contents", no_argument, NULL, 'C' },
	{ "timestamps", no_argument, NULL, 'T' },
	{ "inode-num", no_argument, NULL, 'I' },
	{ "help", no_argument, NULL, 'h' },
	{ "version", no_argument, NULL, 'V' },
};

static const char *short_opts = "OPCTIhV";

static const char *usagestr =
"Usage: sqfsdiff [OPTIONS...] <first> <second>\n"
"\n"
"Compare two squashfs images. In contrast to doing a direct diff of the\n"
"images, this actually parses the filesystems and generates a more\n"
"meaningful difference report.\n"
"\n"
"If only contents are compared, any differences in packed file layout,\n"
"ordering, compression, inode allocation and so on is ignored and the two\n"
"images are considered equal if each directory contains the same entries,\n"
"symlink with the same paths have the same targets, device nodes the same\n"
"device number and files the same size and contents.\n"
"\n"
"A report of any difference is printed to stdout. The exit status is similar\n"
"that of diff(1): 0 means equal, 1 means different, 2 means problem.\n"
"\n"
"Possible options:\n"
"\n"
"  --no-contents, -C           Do not compare file contents.\n"
"  --no-owner, -O              Do not compare file owners.\n"
"  --no-permissions, -P        Do not compare permission bits.\n"
"\n"
"  --timestamps, -T            Compare file timestamps.\n"
"  --inode-num, -I             Compare inode numbers of all files.\n"
"\n"
"  --help, -h                  Print help text and exit.\n"
"  --version, -V               Print version information and exit.\n"
"\n";

int compare_flags = 0;
const char *first_path;
const char *second_path;
sqfs_reader_t sqfs_a;
sqfs_reader_t sqfs_b;

static void process_options(int argc, char **argv)
{
	int i;

	for (;;) {
		i = getopt_long(argc, argv, short_opts, long_opts, NULL);
		if (i == -1)
			break;

		switch (i) {
		case 'O':
			compare_flags |= COMPARE_NO_OWNER;
			break;
		case 'P':
			compare_flags |= COMPARE_NO_PERM;
			break;
		case 'C':
			compare_flags |= COMPARE_NO_CONTENTS;
			break;
		case 'T':
			compare_flags |= COMPARE_TIMESTAMP;
			break;
		case 'I':
			compare_flags |= COMPARE_INODE_NUM;
			break;
		case 'h':
			fputs(usagestr, stdout);
			exit(EXIT_SUCCESS);
		case 'V':
			print_version();
			exit(EXIT_SUCCESS);
		default:
			goto fail_arg;
		}
	}

	if (optind >= argc) {
		fputs("Missing arguments: first filesystem\n", stderr);
		goto fail_arg;
	}

	first_path = argv[optind++];

	if (optind >= argc) {
		fputs("Missing arguments: second filesystem\n", stderr);
		goto fail_arg;
	}

	second_path = argv[optind++];

	if (optind < argc) {
		fputs("Unknown extra arguments\n", stderr);
		goto fail_arg;
	}
	return;
fail_arg:
	fprintf(stderr, "Try `sqfsdiff --help' for more information.\n");
	exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
	int ret = EXIT_FAILURE;

	process_options(argc, argv);

	if (sqfs_reader_open(&sqfs_a, first_path, 0))
		return EXIT_FAILURE;

	if (sqfs_reader_open(&sqfs_b, second_path, 0))
		goto out_sqfs_a;

	ret = node_compare(sqfs_a.fs.root, sqfs_b.fs.root);
	if (ret < 0)
		ret = 2;

	sqfs_reader_close(&sqfs_b);
out_sqfs_a:
	sqfs_reader_close(&sqfs_a);
	return ret;
}
