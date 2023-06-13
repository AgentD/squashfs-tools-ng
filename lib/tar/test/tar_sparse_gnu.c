/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * tar_sparse_gnu.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "io/file.h"
#include "tar/tar.h"
#include "util/test.h"

int main(int argc, char **argv)
{
	tar_header_decoded_t hdr;
	sparse_map_t *sparse;
	sqfs_istream_t *fp;
	int ret;
	(void)argc; (void)argv;

	TEST_ASSERT(chdir(TEST_PATH) == 0);

	ret = istream_open_file(&fp, "sparse-files/gnu-small.tar");
	TEST_EQUAL_I(ret, 0);
	TEST_NOT_NULL(fp);
	TEST_ASSERT(read_header(fp, &hdr) == 0);
	TEST_EQUAL_UI(hdr.mode, S_IFREG | 0644);
	TEST_EQUAL_UI(hdr.uid, 01750);
	TEST_EQUAL_UI(hdr.gid, 01750);
	TEST_EQUAL_UI(hdr.actual_size, 524288);
	TEST_EQUAL_UI(hdr.record_size, 8192);
	TEST_STR_EQUAL(hdr.name, "input.bin");
	TEST_ASSERT(!hdr.unknown_record);

	sparse = hdr.sparse;
	TEST_NOT_NULL(sparse);
	TEST_EQUAL_UI(sparse->offset, 0);
	TEST_EQUAL_UI(sparse->count, 4096);

	sparse = sparse->next;
	TEST_NOT_NULL(sparse);
	TEST_EQUAL_UI(sparse->offset, 262144);
	TEST_EQUAL_UI(sparse->count, 4096);

	sparse = sparse->next;
	TEST_NOT_NULL(sparse);
	TEST_EQUAL_UI(sparse->offset, 524288);
	TEST_EQUAL_UI(sparse->count, 0);

	TEST_NULL(sparse->next);

	clear_header(&hdr);
	sqfs_drop(fp);
	return EXIT_SUCCESS;
}
