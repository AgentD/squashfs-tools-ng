/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * sqfsdiff.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "difftool.h"

static struct option long_opts[] = {
	{ "old", required_argument, NULL, 'a' },
	{ "new", required_argument, NULL, 'b' },
	{ "no-owner", no_argument, NULL, 'O' },
	{ "no-permissions", no_argument, NULL, 'P' },
	{ "no-contents", no_argument, NULL, 'C' },
	{ "timestamps", no_argument, NULL, 'T' },
	{ "inode-num", no_argument, NULL, 'I' },
	{ "super", no_argument, NULL, 'S' },
	{ "extract", required_argument, NULL, 'e' },
	{ "help", no_argument, NULL, 'h' },
	{ "version", no_argument, NULL, 'V' },
};

static const char *short_opts = "a:b:OPCTISe:hV";

static const char *usagestr =
"Usage: sqfsdiff [OPTIONS...] --old,-a <first> --new,-b <second>\n"
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
"  --old, -a <first>           The first of the two images to compare.\n"
"  --new, -b <second>          The second of the two images to compare.\n"
"\n"
"  --no-contents, -C           Do not compare file contents.\n"
"  --no-owner, -O              Do not compare file owners.\n"
"  --no-permissions, -P        Do not compare permission bits.\n"
"\n"
"  --timestamps, -T            Compare file timestamps.\n"
"  --inode-num, -I             Compare inode numbers of all files.\n"
"  --super, -S                 Also compare metadata in super blocks.\n"
"\n"
"  --extract, -e <path>        Extract files that differ to the specified\n"
"                              directory. Contents of the first image end up\n"
"                              in a subdirectory 'a' and of the second image\n"
"                              in a subdirectory 'b'.\n"
"\n"
"  --help, -h                  Print help text and exit.\n"
"  --version, -V               Print version information and exit.\n"
"\n";

int compare_flags = 0;
const char *first_path;
const char *second_path;
sqfs_reader_t sqfs_a;
sqfs_reader_t sqfs_b;
static bool compare_super = false;
static const char *extract_dir;

static void process_options(int argc, char **argv)
{
	int i;

	for (;;) {
		i = getopt_long(argc, argv, short_opts, long_opts, NULL);
		if (i == -1)
			break;

		switch (i) {
		case 'a':
			first_path = optarg;
			break;
		case 'b':
			second_path = optarg;
			break;
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
		case 'S':
			compare_super = true;
			break;
		case 'e':
			compare_flags |= COMPARE_EXTRACT_FILES;
			extract_dir = optarg;
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

	if (first_path == NULL) {
		fputs("Missing arguments: first filesystem\n", stderr);
		goto fail_arg;
	}

	if (second_path == NULL) {
		fputs("Missing arguments: second filesystem\n", stderr);
		goto fail_arg;
	}

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
	int status, ret = 0;

	process_options(argc, argv);

	if (extract_dir != NULL) {
		if (mkdir_p(extract_dir))
			return EXIT_FAILURE;
	}

	if (sqfs_reader_open(&sqfs_a, first_path, 0))
		return 2;

	if (sqfs_reader_open(&sqfs_b, second_path, 0)) {
		status = 2;
		goto out_sqfs_a;
	}

	if (extract_dir != NULL) {
		if (chdir(extract_dir)) {
			perror(extract_dir);
			ret = -1;
			goto out;
		}
	}

	ret = node_compare(sqfs_a.fs.root, sqfs_b.fs.root);
	if (ret != 0)
		goto out;

	if (compare_super) {
		ret = compare_super_blocks(&sqfs_a.super, &sqfs_b.super);
		if (ret != 0)
			goto out;
	}
out:
	if (ret < 0) {
		status = 2;
	} else if (ret > 0) {
		status = 1;
	} else {
		status = 0;
	}
	sqfs_reader_close(&sqfs_b);
out_sqfs_a:
	sqfs_reader_close(&sqfs_a);
	return status;
}
