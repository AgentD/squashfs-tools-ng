/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * tar_xattr_schily.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "util/util.h"
#include "tar.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define STR(x) #x
#define STRVALUE(x) STR(x)

#define TEST_PATH STRVALUE(TESTPATH)

static FILE *open_read(const char *path)
{
	FILE *fp = fopen(path, "rb");

	if (fp == NULL) {
		perror(path);
		exit(EXIT_FAILURE);
	}

	return fp;
}

int main(void)
{
	tar_header_decoded_t hdr;
	char buffer[6];
	FILE *fp;

	assert(chdir(TEST_PATH) == 0);

	fp = open_read("xattr/xattr-schily.tar");
	assert(read_header(fp, &hdr) == 0);
	assert(hdr.sb.st_mode == (S_IFREG | 0644));
	assert(hdr.sb.st_uid == 01750);
	assert(hdr.sb.st_gid == 01750);
	assert(hdr.sb.st_size == 5);
	assert(hdr.sb.st_mtime == 1543094477);
	assert(hdr.mtime == 1543094477);
	assert(strcmp(hdr.name, "input.txt") == 0);
	assert(!hdr.unknown_record);
	assert(read_retry("data0", fp, buffer, 5) == 0);
	buffer[5] = '\0';
	assert(strcmp(buffer, "test\n") == 0);

	assert(hdr.xattr != NULL);
	assert(strcmp(hdr.xattr->key, "user.mime_type") == 0);
	assert(strcmp(hdr.xattr->value, "text/plain") == 0);
	assert(hdr.xattr->next == NULL);

	clear_header(&hdr);
	fclose(fp);
	return EXIT_SUCCESS;
}
