/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * tar_xattr.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "tar.h"
#include "test.h"

int main(void)
{
	tar_header_decoded_t hdr;
	char buffer[6];
	FILE *fp;

	fp = test_open_read(STRVALUE(TESTPATH) "/" STRVALUE(TESTFILE));
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
	return EXIT_SUCCESS;
}
