/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * tar_sparse.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "tar.h"
#include "../test.h"

static void test_case_sparse(const char *path)
{
	tar_header_decoded_t hdr;
	sparse_map_t *sparse;
	istream_t *fp;

	fp = istream_open_file(path);
	TEST_NOT_NULL(fp);
	TEST_ASSERT(read_header(fp, &hdr) == 0);
	TEST_EQUAL_UI(hdr.sb.st_mode, S_IFREG | 0644);
	TEST_EQUAL_UI(hdr.sb.st_uid, 01750);
	TEST_EQUAL_UI(hdr.sb.st_gid, 01750);
	TEST_EQUAL_UI(hdr.sb.st_size, 2097152);
	TEST_EQUAL_UI(hdr.actual_size, 2097152);
	TEST_EQUAL_UI(hdr.record_size, 32768);
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
	TEST_EQUAL_UI(sparse->count, 4096);

	sparse = sparse->next;
	TEST_NOT_NULL(sparse);
	TEST_EQUAL_UI(sparse->offset, 786432);
	TEST_EQUAL_UI(sparse->count, 4096);

	sparse = sparse->next;
	TEST_NOT_NULL(sparse);
	TEST_EQUAL_UI(sparse->offset, 1048576);
	TEST_EQUAL_UI(sparse->count, 4096);

	sparse = sparse->next;
	TEST_NOT_NULL(sparse);
	TEST_EQUAL_UI(sparse->offset, 1310720);
	TEST_EQUAL_UI(sparse->count, 4096);

	sparse = sparse->next;
	TEST_NOT_NULL(sparse);
	TEST_EQUAL_UI(sparse->offset, 1572864);
	TEST_EQUAL_UI(sparse->count, 4096);

	sparse = sparse->next;
	TEST_NOT_NULL(sparse);
	TEST_EQUAL_UI(sparse->offset, 1835008);
	TEST_EQUAL_UI(sparse->count, 4096);

	sparse = sparse->next;
	TEST_NOT_NULL(sparse);
	TEST_EQUAL_UI(sparse->offset, 2097152);
	TEST_EQUAL_UI(sparse->count, 0);

	sparse = sparse->next;
	TEST_NULL(sparse);

	clear_header(&hdr);
	sqfs_destroy(fp);
}

int main(void)
{
	test_case_sparse( STRVALUE(TESTPATH) "/" STRVALUE(TESTFILE) );
	return EXIT_SUCCESS;
}
