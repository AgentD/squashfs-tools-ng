/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * tar_sparse_gnu.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "util/util.h"
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
	sparse_map_t *sparse;
	int fd;

	assert(chdir(TEST_PATH) == 0);

	fd = open_read("sparse-files/gnu-small.tar");
	assert(read_header(fd, &hdr) == 0);
	assert(hdr.sb.st_mode == (S_IFREG | 0644));
	assert(hdr.sb.st_uid == 01750);
	assert(hdr.sb.st_gid == 01750);
	assert(hdr.sb.st_size == 524288);
	assert(hdr.actual_size == 524288);
	assert(hdr.record_size == 8192);
	assert(strcmp(hdr.name, "input.bin") == 0);
	assert(!hdr.unknown_record);

	sparse = hdr.sparse;
	assert(sparse != NULL);
	assert(sparse->offset == 0);
	assert(sparse->count == 4096);

	sparse = sparse->next;
	assert(sparse != NULL);
	assert(sparse->offset == 262144);
	assert(sparse->count == 4096);

	sparse = sparse->next;
	assert(sparse != NULL);
	assert(sparse->offset == 524288);
	assert(sparse->count == 0);

	assert(sparse->next == NULL);

	clear_header(&hdr);
	close(fd);

	fd = open_read("sparse-files/gnu.tar");
	assert(read_header(fd, &hdr) == 0);
	assert(hdr.sb.st_mode == (S_IFREG | 0644));
	assert(hdr.sb.st_uid == 01750);
	assert(hdr.sb.st_gid == 01750);
	assert(hdr.sb.st_size == 2097152);
	assert(hdr.actual_size == 2097152);
	assert(hdr.record_size == 32768);
	assert(strcmp(hdr.name, "input.bin") == 0);
	assert(!hdr.unknown_record);

	sparse = hdr.sparse;
	assert(sparse != NULL);
	assert(sparse->offset == 0);
	assert(sparse->count == 4096);

	sparse = sparse->next;
	assert(sparse != NULL);
	assert(sparse->offset == 262144);
	assert(sparse->count == 4096);

	sparse = sparse->next;
	assert(sparse != NULL);
	assert(sparse->offset == 524288);
	assert(sparse->count == 4096);

	sparse = sparse->next;
	assert(sparse != NULL);
	assert(sparse->offset == 786432);
	assert(sparse->count == 4096);

	sparse = sparse->next;
	assert(sparse != NULL);
	assert(sparse->offset == 1048576);
	assert(sparse->count == 4096);

	sparse = sparse->next;
	assert(sparse != NULL);
	assert(sparse->offset == 1310720);
	assert(sparse->count == 4096);

	sparse = sparse->next;
	assert(sparse != NULL);
	assert(sparse->offset == 1572864);
	assert(sparse->count == 4096);

	sparse = sparse->next;
	assert(sparse != NULL);
	assert(sparse->offset == 1835008);
	assert(sparse->count == 4096);

	sparse = sparse->next;
	assert(sparse != NULL);
	assert(sparse->offset == 2097152);
	assert(sparse->count == 0);

	sparse = sparse->next;
	assert(sparse == NULL);

	clear_header(&hdr);
	close(fd);

	return EXIT_SUCCESS;
}
