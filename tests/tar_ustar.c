/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * tar_ustar.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "util.h"
#include "tar.h"

#include <unistd.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>

#define STR(x) #x
#define STRVALUE(x) STR(x)

#define TEST_PATH STRVALUE(TESTPATH)

static int open_read(const char *path)
{
	int fd = open(path, O_RDONLY);

	if (fd < 0) {
		perror(path);
		exit(EXIT_FAILURE);
	}

	return fd;
}

static const char *filename =
"012345678901234567890123456789/012345678901234567890123456789/"
"012345678901234567890123456789/012345678901234567890123456789/"
"012345678901234567890123456789/input.txt";

int main(void)
{
	tar_header_decoded_t hdr;
	char buffer[6];
	int fd;

	assert(chdir(TEST_PATH) == 0);

	fd = open_read("format-acceptance/ustar.tar");
	assert(read_header(fd, &hdr) == 0);
	assert(hdr.sb.st_mode == (S_IFREG | 0644));
	assert(hdr.sb.st_uid == 01750);
	assert(hdr.sb.st_gid == 01750);
	assert(hdr.sb.st_size == 5);
	assert(hdr.sb.st_mtime == 1542905892);
	assert(hdr.mtime == 1542905892);
	assert(strcmp(hdr.name, "input.txt") == 0);
	assert(!hdr.unknown_record);
	assert(read_retry("data0", fd, buffer, 5) == 0);
	buffer[5] = '\0';
	assert(strcmp(buffer, "test\n") == 0);
	clear_header(&hdr);
	close(fd);

	fd = open_read("format-acceptance/ustar-pre-posix.tar");
	assert(read_header(fd, &hdr) == 0);
	assert(hdr.sb.st_mode == (S_IFREG | 0644));
	assert(hdr.sb.st_uid == 01750);
	assert(hdr.sb.st_gid == 01750);
	assert(hdr.sb.st_size == 5);
	assert(hdr.sb.st_mtime == 1542905892);
	assert(hdr.mtime == 1542905892);
	assert(strcmp(hdr.name, "input.txt") == 0);
	assert(!hdr.unknown_record);
	assert(read_retry("data1", fd, buffer, 5) == 0);
	buffer[5] = '\0';
	assert(strcmp(buffer, "test\n") == 0);
	clear_header(&hdr);
	close(fd);

	fd = open_read("format-acceptance/v7.tar");
	assert(read_header(fd, &hdr) == 0);
	assert(hdr.sb.st_mode == (S_IFREG | 0644));
	assert(hdr.sb.st_uid == 01750);
	assert(hdr.sb.st_gid == 01750);
	assert(hdr.sb.st_size == 5);
	assert(hdr.sb.st_mtime == 1542905892);
	assert(hdr.mtime == 1542905892);
	assert(strcmp(hdr.name, "input.txt") == 0);
	assert(!hdr.unknown_record);
	assert(read_retry("data2", fd, buffer, 5) == 0);
	buffer[5] = '\0';
	assert(strcmp(buffer, "test\n") == 0);
	clear_header(&hdr);
	close(fd);

	fd = open_read("file-size/12-digit.tar");
	assert(read_header(fd, &hdr) == 0);
	assert(hdr.sb.st_mode == (S_IFREG | 0644));
	assert(hdr.sb.st_uid == 01750);
	assert(hdr.sb.st_gid == 01750);
	assert(hdr.sb.st_size == 8589934592);
	assert(hdr.sb.st_mtime == 013375730126);
	assert(hdr.mtime == 013375730126);
	assert(strcmp(hdr.name, "big-file.bin") == 0);
	assert(!hdr.unknown_record);
	clear_header(&hdr);
	close(fd);

	fd = open_read("user-group-largenum/8-digit.tar");
	assert(read_header(fd, &hdr) == 0);
	assert(hdr.sb.st_mode == (S_IFREG | 0644));
	assert(hdr.sb.st_uid == 8388608);
	assert(hdr.sb.st_gid == 8388608);
	assert(hdr.sb.st_size == 5);
	assert(hdr.sb.st_mtime == 013376036700);
	assert(hdr.mtime == 013376036700);
	assert(strcmp(hdr.name, "input.txt") == 0);
	assert(!hdr.unknown_record);
	assert(read_retry("data3", fd, buffer, 5) == 0);
	buffer[5] = '\0';
	assert(strcmp(buffer, "test\n") == 0);
	clear_header(&hdr);
	close(fd);

	fd = open_read("large-mtime/12-digit.tar");
	assert(read_header(fd, &hdr) == 0);
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
	assert(read_retry("data4", fd, buffer, 5) == 0);
	buffer[5] = '\0';
	assert(strcmp(buffer, "test\n") == 0);
	clear_header(&hdr);
	close(fd);

	fd = open_read("long-paths/ustar.tar");
	assert(read_header(fd, &hdr) == 0);
	assert(hdr.sb.st_mode == (S_IFREG | 0644));
	assert(hdr.sb.st_uid == 01750);
	assert(hdr.sb.st_gid == 01750);
	assert(hdr.sb.st_size == 5);
	assert(hdr.sb.st_mtime == 1542909670);
	assert(hdr.mtime == 1542909670);
	assert(strcmp(hdr.name, filename) == 0);
	assert(!hdr.unknown_record);
	assert(read_retry("data5", fd, buffer, 5) == 0);
	buffer[5] = '\0';
	assert(strcmp(buffer, "test\n") == 0);
	clear_header(&hdr);
	close(fd);

	return EXIT_SUCCESS;
}
