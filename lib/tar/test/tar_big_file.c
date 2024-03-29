/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * tar_big_file.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "tar/tar.h"
#include "util/test.h"
#include "sqfs/io.h"

int main(int argc, char **argv)
{
	tar_header_decoded_t hdr;
	sqfs_istream_t *fp;
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
	TEST_EQUAL_UI(hdr.actual_size, 8589934592);
	TEST_EQUAL_UI(hdr.mtime, 1542959190);
	TEST_STR_EQUAL(hdr.name, "big-file.bin");
	TEST_ASSERT(!hdr.unknown_record);
	clear_header(&hdr);
	sqfs_drop(fp);
	return EXIT_SUCCESS;
}
