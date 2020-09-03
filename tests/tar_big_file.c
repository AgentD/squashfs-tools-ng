/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * tar_big_file.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "test_tar.h"

int main(void)
{
	tar_header_decoded_t hdr;
	FILE *fp;

	fp = test_open_read(STRVALUE(TESTPATH) "/" STRVALUE(TESTFILE));
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
	return EXIT_SUCCESS;
}
