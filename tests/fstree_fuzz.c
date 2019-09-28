/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * fstree_fuzz.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "fstree.h"

#include <stdlib.h>
#include <stdio.h>

int main(int argc, char **argv)
{
	int ret = EXIT_FAILURE;
	fstree_t fs;
	FILE *fp;

	if (argc != 2) {
		fputs("Usage: fstree_fuzz <input_file>\n", stderr);
		return EXIT_FAILURE;
	}

	fp = fopen(argv[1], "r");
	if (fp == NULL) {
		perror(argv[1]);
		return EXIT_FAILURE;
	}

	if (fstree_init(&fs, NULL))
		goto out_fp;

	if (fstree_from_file(&fs, argv[1], fp))
		goto out_fs;

	ret = EXIT_SUCCESS;
out_fs:
	fstree_cleanup(&fs);
out_fp:
	fclose(fp);
	return ret;
}
