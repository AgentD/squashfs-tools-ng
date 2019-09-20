/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * sqfsdiff.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "sqfsdiff.h"

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
"ordering, compression, inode meta data and so on is ignored and the two\n"
"images are considered equal if each directory contains the same entries,\n"
"symlink with the same paths have the same targets, device nodes the same\n"
"device number and files the same size and contents.\n"
"\n"
"A report of any difference is printed to stdout. The exit status is similar\n"
"that of diff(1): 0 means equal, 1 means different, 2 means problem.\n"
"\n"
"Possible options:\n"
"\n"
"  --old, -a <first>           The first of the two filesystems to compare.\n"
"  --new, -b <second>          The second of the two filesystems to compare.\n"
"\n"
"  --no-contents, -C           Do not compare file contents.\n"
"  --no-owner, -O              Do not compare file owners.\n"
"  --no-permissions, -P        Do not compare permission bits.\n"
"\n"
"  --timestamps, -T            Compare file timestamps.\n"
"  --inode-num, -I             Compare inode numbers of all files.\n"
"  --super, -S                 Also compare meta data in super blocks.\n"
"\n"
"  --extract, -e <path>        Extract files that differ to the specified\n"
"                              directory. Contents of the first filesystem\n"
"                              end up in a subdirectory 'old' and of the\n"
"                              second filesystem in a subdirectory 'new'.\n"
"\n"
"  --help, -h                  Print help text and exit.\n"
"  --version, -V               Print version information and exit.\n"
"\n";

void process_options(sqfsdiff_t *sd, int argc, char **argv)
{
	int i;

	for (;;) {
		i = getopt_long(argc, argv, short_opts, long_opts, NULL);
		if (i == -1)
			break;

		switch (i) {
		case 'a':
			sd->old_path = optarg;
			break;
		case 'b':
			sd->new_path = optarg;
			break;
		case 'O':
			sd->compare_flags |= COMPARE_NO_OWNER;
			break;
		case 'P':
			sd->compare_flags |= COMPARE_NO_PERM;
			break;
		case 'C':
			sd->compare_flags |= COMPARE_NO_CONTENTS;
			break;
		case 'T':
			sd->compare_flags |= COMPARE_TIMESTAMP;
			break;
		case 'I':
			sd->compare_flags |= COMPARE_INODE_NUM;
			break;
		case 'S':
			sd->compare_super = true;
			break;
		case 'e':
			sd->compare_flags |= COMPARE_EXTRACT_FILES;
			sd->extract_dir = optarg;
			break;
		case 'h':
			fputs(usagestr, stdout);
			exit(0);
		case 'V':
			print_version();
			exit(0);
		default:
			goto fail_arg;
		}
	}

	if (sd->old_path == NULL) {
		fputs("Missing arguments: first filesystem\n", stderr);
		goto fail_arg;
	}

	if (sd->new_path == NULL) {
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
	exit(2);
}
