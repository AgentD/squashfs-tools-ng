/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * xattr_benchmark.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "compat.h"
#include "common.h"

#include "sqfs/xattr_writer.h"
#include "sqfs/xattr.h"

#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <stdio.h>

static struct option long_opts[] = {
	{ "block-count", required_argument, NULL, 'b' },
	{ "groups-size", required_argument, NULL, 'g' },
	{ "version", no_argument, NULL, 'V' },
	{ "help", no_argument, NULL, 'h' },
	{ NULL, 0, NULL, 0 },
};

static const char *short_opts = "g:b:hV";

static const char *help_string =
"Usage: xattr_benchmark [OPTIONS...]\n"
"\n"
"Possible options:\n"
"\n"
"  --block-count, -b <count>  How many unique xattr blocks to generate.\n"
"  --group-size, -g <count>   Number of key-value pairs to generate for each\n"
"                             xattr block.\n"
"\n";

int main(int argc, char **argv)
{
	long blkidx, grpidx, block_count = 0, group_size = 0;
	sqfs_xattr_writer_t *xwr;
	sqfs_u32 id;
	int ret;

	for (;;) {
		int i = getopt_long(argc, argv, short_opts, long_opts, NULL);
		if (i == -1)
			break;

		switch (i) {
		case 'b':
			block_count = strtol(optarg, NULL, 0);
			break;
		case 'g':
			group_size = strtol(optarg, NULL, 0);
			break;
		case 'h':
			fputs(help_string, stdout);
			return EXIT_SUCCESS;
		case 'V':
			print_version("xattr_benchmark");
			return EXIT_SUCCESS;
		default:
			goto fail_arg;
		}
	}

	if (block_count <= 0) {
		fputs("A block count > 0 must be specified.\n", stderr);
		goto fail_arg;
	}

	if (group_size <= 0) {
		fputs("A group size > 0 must be specified.\n", stderr);
		goto fail_arg;
	}

	/* setup writer */
	xwr = sqfs_xattr_writer_create(0);

	/* generate blocks */
	for (blkidx = 0; blkidx < block_count; ++blkidx) {
		ret = sqfs_xattr_writer_begin(xwr, 0);
		if (ret < 0) {
			sqfs_perror(NULL, "begin xattr block", ret);
			goto fail;
		}

		for (grpidx = 0; grpidx < group_size; ++grpidx) {
			char key[64], value[64];

			snprintf(key, sizeof(key), "user.group%ld.key%ld",
				 blkidx, grpidx);

			snprintf(value, sizeof(value), "group%ld/value%ld",
				 blkidx, grpidx);

			ret = sqfs_xattr_writer_add(xwr, key, value,
						    strlen(value));

			if (ret < 0) {
				sqfs_perror(NULL, "add to xattr block", ret);
				goto fail;
			}
		}

		ret = sqfs_xattr_writer_end(xwr, &id);
		if (ret < 0) {
			sqfs_perror(NULL, "end xattr block", ret);
			goto fail;
		}
	}

	/* cleanup */
	sqfs_destroy(xwr);
	return EXIT_SUCCESS;
fail:
	sqfs_destroy(xwr);
	return EXIT_FAILURE;
fail_arg:
	fputs("Try `xattr_benchmark --help' for more information.\n", stderr);
	return EXIT_FAILURE;
}
