/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * tar_pax.c
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

static const char *filename =
"012345678901234567890123456789/012345678901234567890123456789/"
"012345678901234567890123456789/012345678901234567890123456789/"
"012345678901234567890123456789/input.txt";

int main(void)
{
	tar_header_decoded_t hdr;
	char buffer[6];
	FILE *fp;

	assert(chdir(TEST_PATH) == 0);

	fp = open_read("format-acceptance/pax.tar");
	assert(read_header(fp, &hdr) == 0);
	assert(hdr.sb.st_mode == (S_IFREG | 0644));
	assert(hdr.sb.st_uid == 01750);
	assert(hdr.sb.st_gid == 01750);
	assert(hdr.sb.st_size == 5);
	assert(hdr.sb.st_mtime == 1542905892);
	assert(hdr.mtime == 1542905892);
	assert(strcmp(hdr.name, "input.txt") == 0);
	assert(!hdr.unknown_record);
	assert(read_retry("data0", fp, buffer, 5) == 0);
	buffer[5] = '\0';
	assert(strcmp(buffer, "test\n") == 0);
	clear_header(&hdr);
	fclose(fp);

	fp = open_read("file-size/pax.tar");
	assert(read_header(fp, &hdr) == 0);
	assert(hdr.sb.st_mode == (S_IFREG | 0644));
	assert(hdr.sb.st_uid == 01750);
	assert(hdr.sb.st_gid == 01750);
	assert(hdr.sb.st_size == 8589934592);
	assert(hdr.sb.st_mtime == 1542959190);
	assert(hdr.mtime == 1542959190);
	assert(strcmp(hdr.name, "big-file.bin") == 0);
	assert(!hdr.unknown_record);
	clear_header(&hdr);
	fclose(fp);

	fp = open_read("user-group-largenum/pax.tar");
	assert(read_header(fp, &hdr) == 0);
	assert(hdr.sb.st_mode == (S_IFREG | 0644));
	assert(hdr.sb.st_uid == 2147483648);
	assert(hdr.sb.st_gid == 2147483648);
	assert(hdr.sb.st_size == 5);
	assert(hdr.sb.st_mtime == 013376036700);
	assert(hdr.mtime == 013376036700);
	assert(strcmp(hdr.name, "input.txt") == 0);
	assert(!hdr.unknown_record);
	assert(read_retry("data1", fp, buffer, 5) == 0);
	buffer[5] = '\0';
	assert(strcmp(buffer, "test\n") == 0);
	clear_header(&hdr);
	fclose(fp);

	fp = open_read("large-mtime/pax.tar");
	assert(read_header(fp, &hdr) == 0);
	assert(hdr.sb.st_mode == (S_IFREG | 0644));
	assert(hdr.sb.st_uid == 01750);
	assert(hdr.sb.st_gid == 01750);
	assert(hdr.sb.st_size == 5);
#if SIZEOF_TIME_T < 8
	assert(hdr.sb.st_mtime == INT32_MAX);
#else
	assert(hdr.sb.st_mtime == 8589934592L);
#endif
	assert(hdr.mtime == 8589934592L);
	assert(strcmp(hdr.name, "input.txt") == 0);
	assert(!hdr.unknown_record);
	assert(read_retry("data2", fp, buffer, 5) == 0);
	buffer[5] = '\0';
	assert(strcmp(buffer, "test\n") == 0);
	clear_header(&hdr);
	fclose(fp);

	fp = open_read("negative-mtime/pax.tar");
	assert(read_header(fp, &hdr) == 0);
	assert(hdr.sb.st_mode == (S_IFREG | 0644));
	assert(hdr.sb.st_uid == 01750);
	assert(hdr.sb.st_gid == 01750);
	assert(hdr.sb.st_size == 5);
	assert(hdr.sb.st_mtime == -315622800);
	assert(hdr.mtime == -315622800);
	assert(strcmp(hdr.name, "input.txt") == 0);
	assert(!hdr.unknown_record);
	assert(read_retry("data3", fp, buffer, 5) == 0);
	buffer[5] = '\0';
	assert(strcmp(buffer, "test\n") == 0);
	clear_header(&hdr);
	fclose(fp);

	fp = open_read("long-paths/pax.tar");
	assert(read_header(fp, &hdr) == 0);
	assert(hdr.sb.st_mode == (S_IFREG | 0644));
	assert(hdr.sb.st_uid == 01750);
	assert(hdr.sb.st_gid == 01750);
	assert(hdr.sb.st_size == 5);
	assert(hdr.sb.st_mtime == 1542909670);
	assert(hdr.mtime == 1542909670);
	assert(strcmp(hdr.name, filename) == 0);
	assert(!hdr.unknown_record);
	assert(read_retry("data4", fp, buffer, 5) == 0);
	buffer[5] = '\0';
	assert(strcmp(buffer, "test\n") == 0);
	clear_header(&hdr);
	fclose(fp);

	return EXIT_SUCCESS;
}
