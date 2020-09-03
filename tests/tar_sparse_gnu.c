/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * tar_sparse_gnu.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "test_tar.h"

int main(void)
{
	tar_header_decoded_t hdr;
	sparse_map_t *sparse;
	FILE *fp;

	TEST_ASSERT(chdir(TEST_PATH) == 0);

	fp = test_open_read("sparse-files/gnu-small.tar");
	TEST_ASSERT(read_header(fp, &hdr) == 0);
	TEST_EQUAL_UI(hdr.sb.st_mode, S_IFREG | 0644);
	TEST_EQUAL_UI(hdr.sb.st_uid, 01750);
	TEST_EQUAL_UI(hdr.sb.st_gid, 01750);
	TEST_EQUAL_UI(hdr.sb.st_size, 524288);
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
	fclose(fp);

	test_case_sparse("sparse-files/gnu.tar");
	return EXIT_SUCCESS;
}
