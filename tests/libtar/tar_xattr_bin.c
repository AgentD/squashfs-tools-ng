/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * tar_xattr_bin.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "tar.h"
#include "../test.h"

static const uint8_t value[] = {
	0x00, 0x00, 0x00, 0x02,
	0x00, 0x30, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
};

int main(void)
{
	tar_header_decoded_t hdr;
	char buffer[6];
	istream_t *fp;

	fp = istream_open_file(STRVALUE(TESTPATH) "/" STRVALUE(TESTFILE));
	TEST_NOT_NULL(fp);
	TEST_ASSERT(read_header(fp, &hdr) == 0);
	TEST_EQUAL_UI(hdr.sb.st_mode, S_IFREG | 0644);
	TEST_EQUAL_UI(hdr.sb.st_uid, 01750);
	TEST_EQUAL_UI(hdr.sb.st_gid, 01750);
	TEST_EQUAL_UI(hdr.sb.st_size, 5);
	TEST_EQUAL_UI(hdr.sb.st_mtime, 1543094477);
	TEST_EQUAL_UI(hdr.mtime, 1543094477);
	TEST_STR_EQUAL(hdr.name, "input.txt");
	TEST_ASSERT(!hdr.unknown_record);
	TEST_ASSERT(istream_read(fp, buffer, 5) == 5);
	buffer[5] = '\0';
	TEST_STR_EQUAL(buffer, "test\n");

	TEST_NOT_NULL(hdr.xattr);
	TEST_STR_EQUAL(hdr.xattr->key, "security.capability");
	TEST_EQUAL_UI(hdr.xattr->value_len, sizeof(value));
	TEST_ASSERT(memcmp(hdr.xattr->value, value, sizeof(value)) == 0);
	TEST_NULL(hdr.xattr->next);

	clear_header(&hdr);
	sqfs_destroy(fp);
	return EXIT_SUCCESS;
}
