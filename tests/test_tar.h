/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * test_tar.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef TEST_TAR_H
#define TEST_TAR_H

#include "config.h"
#include "tar.h"
#include "test.h"

#define STR(x) #x
#define STRVALUE(x) STR(x)

#define TEST_PATH STRVALUE(TESTPATH)

static ATTRIB_UNUSED void testcase_simple(const char *path, sqfs_s64 ts,
					  sqfs_u32 uid, sqfs_u32 gid,
					  const char *fname)
{
	tar_header_decoded_t hdr;
	char buffer[6];
	FILE *fp;

	fp = test_open_read(path);
	TEST_ASSERT(read_header(fp, &hdr) == 0);
	TEST_EQUAL_UI(hdr.sb.st_mode, S_IFREG | 0644);
	TEST_EQUAL_UI(hdr.sb.st_uid, uid);
	TEST_EQUAL_UI(hdr.sb.st_gid, gid);
	TEST_EQUAL_UI(hdr.sb.st_size, 5);

	if (sizeof(time_t) < sizeof(ts) && ts > INT32_MAX) {
		TEST_EQUAL_UI(hdr.sb.st_mtime, INT32_MAX);
	} else {
		TEST_EQUAL_UI(hdr.sb.st_mtime, ts);
	}

	TEST_EQUAL_UI(hdr.mtime, ts);
	TEST_STR_EQUAL(hdr.name, fname);
	TEST_ASSERT(!hdr.unknown_record);

	TEST_ASSERT(read_retry("tar data", fp, buffer, 5) == 0);
	buffer[5] = '\0';
	TEST_STR_EQUAL(buffer, "test\n");
	clear_header(&hdr);
	fclose(fp);

}

static ATTRIB_UNUSED void test_case_file_size(const char *path)
{
	tar_header_decoded_t hdr;
	FILE *fp;

	fp = test_open_read(path);
	TEST_ASSERT(read_header(fp, &hdr) == 0);
	TEST_EQUAL_UI(hdr.sb.st_mode, S_IFREG | 0644);
	TEST_EQUAL_UI(hdr.sb.st_uid, 01750);
	TEST_EQUAL_UI(hdr.sb.st_gid, 01750);
	TEST_EQUAL_UI(hdr.sb.st_size, 8589934592);
	TEST_EQUAL_UI(hdr.sb.st_mtime, 1542959190);
	TEST_EQUAL_UI(hdr.mtime, 1542959190);
	TEST_STR_EQUAL(hdr.name, "big-file.bin");
	TEST_ASSERT(!hdr.unknown_record);
	clear_header(&hdr);
	fclose(fp);
}

static ATTRIB_UNUSED void test_case_sparse(const char *path)
{
	tar_header_decoded_t hdr;
	sparse_map_t *sparse;
	FILE *fp;

	fp = test_open_read(path);
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
	fclose(fp);
}

static ATTRIB_UNUSED void test_case_xattr_simple(const char *path)
{
	tar_header_decoded_t hdr;
	char buffer[6];
	FILE *fp;

	fp = test_open_read(path);
	TEST_ASSERT(read_header(fp, &hdr) == 0);
	TEST_EQUAL_UI(hdr.sb.st_mode, S_IFREG | 0644);
	TEST_EQUAL_UI(hdr.sb.st_uid, 01750);
	TEST_EQUAL_UI(hdr.sb.st_gid, 01750);
	TEST_EQUAL_UI(hdr.sb.st_size, 5);
	TEST_EQUAL_UI(hdr.sb.st_mtime, 1543094477);
	TEST_EQUAL_UI(hdr.mtime, 1543094477);
	TEST_STR_EQUAL(hdr.name, "input.txt");
	TEST_ASSERT(!hdr.unknown_record);
	TEST_ASSERT(read_retry("reading tar data", fp, buffer, 5) == 0);
	buffer[5] = '\0';
	TEST_STR_EQUAL(buffer, "test\n");

	TEST_NOT_NULL(hdr.xattr);
	TEST_STR_EQUAL(hdr.xattr->key, "user.mime_type");
	TEST_STR_EQUAL((const char *)hdr.xattr->value, "text/plain");
	TEST_EQUAL_UI(hdr.xattr->value_len, 10);
	TEST_NULL(hdr.xattr->next);

	clear_header(&hdr);
	fclose(fp);
}

#endif /* TEST_TAR_H */
