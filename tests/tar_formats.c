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
	char buffer[6];
	int fd;

	assert(chdir(TEST_PATH) == 0);

	fd = open_read("format-acceptance/pax.tar");
	assert(read_header(fd, &hdr) == 0);
	assert(hdr.sb.st_mode == (S_IFREG | 0644));
	assert(hdr.sb.st_uid == 01750);
	assert(hdr.sb.st_gid == 01750);
	assert(hdr.sb.st_size == 5);
	assert(hdr.sb.st_mtime == 1542905892);
	assert(hdr.sb.st_atime == 1542905911);
	assert(hdr.sb.st_ctime == 1542905892);
	assert(strcmp(hdr.name, "input.txt") == 0);
	assert(!hdr.unknown_record);
	assert(read_retry(fd, buffer, 5) == 5);
	buffer[5] = '\0';
	assert(strcmp(buffer, "test\n") == 0);
	clear_header(&hdr);
	close(fd);

	fd = open_read("format-acceptance/ustar.tar");
	assert(read_header(fd, &hdr) == 0);
	assert(hdr.sb.st_mode == (S_IFREG | 0644));
	assert(hdr.sb.st_uid == 01750);
	assert(hdr.sb.st_gid == 01750);
	assert(hdr.sb.st_size == 5);
	assert(hdr.sb.st_mtime == 1542905892);
	assert(hdr.sb.st_atime == 1542905892);
	assert(hdr.sb.st_ctime == 1542905892);
	assert(strcmp(hdr.name, "input.txt") == 0);
	assert(!hdr.unknown_record);
	assert(read_retry(fd, buffer, 5) == 5);
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
	assert(hdr.sb.st_atime == 1542905892);
	assert(hdr.sb.st_ctime == 1542905892);
	assert(strcmp(hdr.name, "input.txt") == 0);
	assert(!hdr.unknown_record);
	assert(read_retry(fd, buffer, 5) == 5);
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
	assert(hdr.sb.st_atime == 1542905892);
	assert(hdr.sb.st_ctime == 1542905892);
	assert(strcmp(hdr.name, "input.txt") == 0);
	assert(!hdr.unknown_record);
	assert(read_retry(fd, buffer, 5) == 5);
	buffer[5] = '\0';
	assert(strcmp(buffer, "test\n") == 0);
	clear_header(&hdr);
	close(fd);

	fd = open_read("format-acceptance/gnu.tar");
	assert(read_header(fd, &hdr) == 0);
	assert(hdr.sb.st_mode == (S_IFREG | 0644));
	assert(hdr.sb.st_uid == 01750);
	assert(hdr.sb.st_gid == 01750);
	assert(hdr.sb.st_size == 5);
	assert(hdr.sb.st_mtime == 1542905892);
	assert(hdr.sb.st_atime == 1542905892);
	assert(hdr.sb.st_ctime == 1542905892);
	assert(strcmp(hdr.name, "input.txt") == 0);
	assert(!hdr.unknown_record);
	assert(read_retry(fd, buffer, 5) == 5);
	buffer[5] = '\0';
	assert(strcmp(buffer, "test\n") == 0);
	clear_header(&hdr);
	close(fd);

	fd = open_read("format-acceptance/gnu-g.tar");
	assert(read_header(fd, &hdr) == 0);
	assert(hdr.sb.st_mode == (S_IFREG | 0644));
	assert(hdr.sb.st_uid == 01750);
	assert(hdr.sb.st_gid == 01750);
	assert(hdr.sb.st_size == 5);
	assert(hdr.sb.st_mtime == 013375560044);
	assert(hdr.sb.st_atime == 013375561762);
	assert(hdr.sb.st_ctime == 013375561750);
	assert(strcmp(hdr.name, "input.txt") == 0);
	assert(!hdr.unknown_record);
	assert(read_retry(fd, buffer, 5) == 5);
	buffer[5] = '\0';
	assert(strcmp(buffer, "test\n") == 0);
	clear_header(&hdr);
	close(fd);

	return EXIT_SUCCESS;
}
