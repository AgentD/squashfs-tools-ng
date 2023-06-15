/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * tar_xattr.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "tar/tar.h"
#include "util/test.h"
#include "sqfs/xattr.h"
#include "sqfs/io.h"

int main(int argc, char **argv)
{
	tar_header_decoded_t hdr;
	sqfs_istream_t *fp;
	char buffer[6];
	int ret;
	(void)argc; (void)argv;

	ret = sqfs_istream_open_file(&fp,
				     STRVALUE(TESTPATH) "/" STRVALUE(TESTFILE),
				     0);
	TEST_EQUAL_I(ret, 0);
	TEST_NOT_NULL(fp);
	TEST_ASSERT(read_header(fp, &hdr) == 0);
	TEST_EQUAL_UI(hdr.mode, S_IFREG | 0644);
	TEST_EQUAL_UI(hdr.uid, 01750);
	TEST_EQUAL_UI(hdr.gid, 01750);
	TEST_EQUAL_UI(hdr.actual_size, 5);
	TEST_EQUAL_UI(hdr.mtime, 1543094477);
	TEST_STR_EQUAL(hdr.name, "input.txt");
	TEST_ASSERT(!hdr.unknown_record);
	TEST_ASSERT(sqfs_istream_read(fp, buffer, 5) == 5);
	buffer[5] = '\0';
	TEST_STR_EQUAL(buffer, "test\n");

	TEST_NOT_NULL(hdr.xattr);
	TEST_STR_EQUAL(hdr.xattr->key, "user.mime_type");
	TEST_STR_EQUAL((const char *)hdr.xattr->value, "text/plain");
	TEST_EQUAL_UI(hdr.xattr->value_len, 10);
	TEST_NULL(hdr.xattr->next);

	clear_header(&hdr);
	sqfs_drop(fp);
	return EXIT_SUCCESS;
}
