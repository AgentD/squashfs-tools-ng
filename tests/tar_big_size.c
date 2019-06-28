/* SPDX-License-Identifier: GPL-3.0-or-later */
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

int main(void)
{
	tar_header_decoded_t hdr;
	int fd;

	assert(chdir(TEST_PATH) == 0);

	fd = open_read("file-size/pax.tar");
	assert(read_header(fd, &hdr) == 0);
	assert(hdr.sb.st_mode == (S_IFREG | 0644));
	assert(hdr.sb.st_uid == 01750);
	assert(hdr.sb.st_gid == 01750);
	assert(hdr.sb.st_size == 8589934592);
	assert(hdr.sb.st_mtime == 1542959190);
	assert(hdr.sb.st_atime == 1542959522);
	assert(hdr.sb.st_ctime == 1542959190);
	assert(strcmp(hdr.name, "big-file.bin") == 0);
	assert(!hdr.unknown_record);
	clear_header(&hdr);
	close(fd);

	fd = open_read("file-size/gnu.tar");
	assert(read_header(fd, &hdr) == 0);
	assert(hdr.sb.st_mode == (S_IFREG | 0644));
	assert(hdr.sb.st_uid == 01750);
	assert(hdr.sb.st_gid == 01750);
	assert(hdr.sb.st_size == 8589934592);
	assert(hdr.sb.st_mtime == 013375730126);
	assert(hdr.sb.st_atime == 013375730126);
	assert(hdr.sb.st_ctime == 013375730126);
	assert(strcmp(hdr.name, "big-file.bin") == 0);
	assert(!hdr.unknown_record);
	clear_header(&hdr);
	close(fd);

	fd = open_read("file-size/12-digit.tar");
	assert(read_header(fd, &hdr) == 0);
	assert(hdr.sb.st_mode == (S_IFREG | 0644));
	assert(hdr.sb.st_uid == 01750);
	assert(hdr.sb.st_gid == 01750);
	assert(hdr.sb.st_size == 8589934592);
	assert(hdr.sb.st_mtime == 013375730126);
	assert(hdr.sb.st_atime == 013375730126);
	assert(hdr.sb.st_ctime == 013375730126);
	assert(strcmp(hdr.name, "big-file.bin") == 0);
	assert(!hdr.unknown_record);
	clear_header(&hdr);
	close(fd);

	return EXIT_SUCCESS;
}
