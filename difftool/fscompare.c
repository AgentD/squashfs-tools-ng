/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * fscompare.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "difftool.h"

static struct option long_opts[] = {
	{ "no-owner", no_argument, NULL, 'O' },
	{ "no-permissions", no_argument, NULL, 'P' },
	{ "timestamps", no_argument, NULL, 'T' },
	{ "help", no_argument, NULL, 'h' },
	{ "version", no_argument, NULL, 'V' },
};

static const char *short_opts = "OPThV";

static const char *usagestr =
"Usage: fscompare [OPTIONS...] <first> <second>\n"
"\n"
"Compare two directories, making sure that each contains the same entries.\n"
"If an entry is a directory, comparison recurses into the directory.\n"
"\n"
"Possible options:\n"
"\n"
"  --no-owner, -O              Do not compare file owners.\n"
"  --no-permissions, -P        Do not compare permission bits.\n"
"\n"
"  --timestamps, -T            Compare file timestamps.\n"
"\n"
"  --help, -h                  Print help text and exit.\n"
"  --version, -V               Print version information and exit.\n"
"\n";

const char *first_path;
const char *second_path;
int compare_flags = 0;

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
		case 'T':
			compare_flags |= COMPARE_TIMESTAMP;
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
		fputs("Missing arguments: first directory\n", stderr);
		goto fail_arg;
	}

	first_path = argv[optind++];

	if (optind >= argc) {
		fputs("Missing arguments: second directory\n", stderr);
		goto fail_arg;
	}

	second_path = argv[optind++];

	if (optind < argc) {
		fputs("Unknown extra arguments\n", stderr);
		goto fail_arg;
	}
	return;
fail_arg:
	fprintf(stderr, "Try `fscompare --help' for more information.\n");
	exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
	int ret = EXIT_FAILURE;
	fstree_t afs, bfs;

	process_options(argc, argv);

	if (fstree_init(&afs, 512, NULL))
		return EXIT_FAILURE;

	if (fstree_init(&bfs, 512, NULL))
		goto out_afs;

	if (fstree_from_dir(&afs, first_path, DIR_SCAN_KEEP_TIME))
		goto out_bfs;

	if (fstree_from_dir(&bfs, second_path, DIR_SCAN_KEEP_TIME))
		goto out_bfs;

	tree_node_sort_recursive(afs.root);
	tree_node_sort_recursive(bfs.root);

	if (node_compare(afs.root, bfs.root) == 0)
		ret = EXIT_SUCCESS;

out_bfs:
	fstree_cleanup(&bfs);
out_afs:
	fstree_cleanup(&afs);
	return ret;
}
