/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * tar_target_filled.c
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
	char buffer[16];
	int ret;
	(void)argc; (void)argv;

	TEST_ASSERT(chdir(TEST_PATH) == 0);

	ret = sqfs_istream_open_file(&fp, "format-acceptance/link_filled.tar");
	TEST_EQUAL_I(ret, 0);
	TEST_NOT_NULL(fp);

	/* "deep" directory hierarchy containg 2 files */
	TEST_ASSERT(read_header(fp, &hdr) == 0);
	TEST_EQUAL_UI(hdr.mode, S_IFDIR | 0777);
	TEST_STR_EQUAL(hdr.name, "20_characters_here01/");
	clear_header(&hdr);

	TEST_ASSERT(read_header(fp, &hdr) == 0);
	TEST_EQUAL_UI(hdr.mode, S_IFDIR | 0777);
	TEST_STR_EQUAL(hdr.name, "20_characters_here01/20_characters_here02/");
	clear_header(&hdr);

	TEST_ASSERT(read_header(fp, &hdr) == 0);
	TEST_EQUAL_UI(hdr.mode, S_IFDIR | 0777);
	TEST_STR_EQUAL(hdr.name, "20_characters_here01/20_characters_here02/"
		       "20_characters_here03/");
	clear_header(&hdr);

	TEST_ASSERT(read_header(fp, &hdr) == 0);
	TEST_EQUAL_UI(hdr.mode, S_IFDIR | 0777);
	TEST_STR_EQUAL(hdr.name, "20_characters_here01/20_characters_here02/"
		       "20_characters_here03/20_characters_here04/");
	clear_header(&hdr);

	TEST_ASSERT(read_header(fp, &hdr) == 0);
	TEST_EQUAL_UI(hdr.mode, S_IFREG | 0777);
	TEST_STR_EQUAL(hdr.name, "20_characters_here01/20_characters_here02/"
		       "20_characters_here03/20_characters_here04/"
		       "errored_file_tst");
	TEST_EQUAL_UI(hdr.actual_size, 5);
	TEST_ASSERT(sqfs_istream_read(fp, buffer, 5) == 5);
	buffer[5] = '\0';
	TEST_STR_EQUAL(buffer, "test\n");
	TEST_ASSERT(sqfs_istream_skip(fp, 512 - 5) == 0);
	clear_header(&hdr);

	TEST_ASSERT(read_header(fp, &hdr) == 0);
	TEST_EQUAL_UI(hdr.mode, S_IFREG | 0777);
	TEST_STR_EQUAL(hdr.name, "20_characters_here01/20_characters_here02/"
		       "20_characters_here03/20_characters_here04/"
		       "some_test_file");
	TEST_EQUAL_UI(hdr.actual_size, 5);
	TEST_ASSERT(sqfs_istream_read(fp, buffer, 5) == 5);
	buffer[5] = '\0';
	TEST_STR_EQUAL(buffer, "test\n");
	TEST_ASSERT(sqfs_istream_skip(fp, 512 - 5) == 0);
	clear_header(&hdr);

	/* "deep" directory hierarchy containg a hard link */
	TEST_ASSERT(read_header(fp, &hdr) == 0);
	TEST_EQUAL_UI(hdr.mode, S_IFDIR | 0777);
	TEST_STR_EQUAL(hdr.name, "20CharsForLnkTest001/");
	clear_header(&hdr);

	TEST_ASSERT(read_header(fp, &hdr) == 0);
	TEST_EQUAL_UI(hdr.mode, S_IFDIR | 0777);
	TEST_STR_EQUAL(hdr.name, "20CharsForLnkTest001/20CharsForLnkTest002/");
	clear_header(&hdr);

	TEST_ASSERT(read_header(fp, &hdr) == 0);
	TEST_EQUAL_UI(hdr.mode, S_IFDIR | 0777);
	TEST_STR_EQUAL(hdr.name, "20CharsForLnkTest001/20CharsForLnkTest002/"
		       "20CharsForLnkTest003/");
	clear_header(&hdr);

	TEST_ASSERT(read_header(fp, &hdr) == 0);
	TEST_EQUAL_UI(hdr.mode, S_IFDIR | 0777);
	TEST_STR_EQUAL(hdr.name, "20CharsForLnkTest001/20CharsForLnkTest002/"
		       "20CharsForLnkTest003/20CharsForLnkTest004/");
	clear_header(&hdr);

	TEST_ASSERT(read_header(fp, &hdr) == 0);
	TEST_STR_EQUAL(hdr.name, "20CharsForLnkTest001/20CharsForLnkTest002/"
		       "20CharsForLnkTest003/20CharsForLnkTest004/"
		       "01234567890123456789");
	TEST_ASSERT(hdr.is_hard_link);

	TEST_STR_EQUAL(hdr.link_target, "20_characters_here01/"
		       "20_characters_here02/20_characters_here03/"
		       "20_characters_here04/errored_file_tst");
	clear_header(&hdr);

	/* end of file */
	TEST_ASSERT(read_header(fp, &hdr) > 0);
	sqfs_drop(fp);
	return EXIT_SUCCESS;
}
