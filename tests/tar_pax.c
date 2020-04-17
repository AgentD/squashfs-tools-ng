/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * tar_pax.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "tar.h"
#include "test.h"

#define STR(x) #x
#define STRVALUE(x) STR(x)

#define TEST_PATH STRVALUE(TESTPATH)

static const char *filename =
"012345678901234567890123456789/012345678901234567890123456789/"
"012345678901234567890123456789/012345678901234567890123456789/"
"012345678901234567890123456789/input.txt";

int main(void)
{
	tar_header_decoded_t hdr;
	char buffer[6];
	FILE *fp;

	TEST_ASSERT(chdir(TEST_PATH) == 0);

	fp = test_open_read("format-acceptance/pax.tar");
	TEST_ASSERT(read_header(fp, &hdr) == 0);
	TEST_EQUAL_UI(hdr.sb.st_mode, S_IFREG | 0644);
	TEST_EQUAL_UI(hdr.sb.st_uid, 01750);
	TEST_EQUAL_UI(hdr.sb.st_gid, 01750);
	TEST_EQUAL_UI(hdr.sb.st_size, 5);
	TEST_EQUAL_UI(hdr.sb.st_mtime, 1542905892);
	TEST_EQUAL_UI(hdr.mtime, 1542905892);
	TEST_STR_EQUAL(hdr.name, "input.txt");
	TEST_ASSERT(!hdr.unknown_record);
	TEST_ASSERT(read_retry("data0", fp, buffer, 5) == 0);
	buffer[5] = '\0';
	TEST_STR_EQUAL(buffer, "test\n");
	clear_header(&hdr);
	fclose(fp);

	fp = test_open_read("file-size/pax.tar");
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

	fp = test_open_read("user-group-largenum/pax.tar");
	TEST_ASSERT(read_header(fp, &hdr) == 0);
	TEST_EQUAL_UI(hdr.sb.st_mode, S_IFREG | 0644);
	TEST_EQUAL_UI(hdr.sb.st_uid, 2147483648);
	TEST_EQUAL_UI(hdr.sb.st_gid, 2147483648);
	TEST_EQUAL_UI(hdr.sb.st_size, 5);
	TEST_EQUAL_UI(hdr.sb.st_mtime, 013376036700);
	TEST_EQUAL_UI(hdr.mtime, 013376036700);
	TEST_STR_EQUAL(hdr.name, "input.txt");
	TEST_ASSERT(!hdr.unknown_record);
	TEST_ASSERT(read_retry("data1", fp, buffer, 5) == 0);
	buffer[5] = '\0';
	TEST_STR_EQUAL(buffer, "test\n");
	clear_header(&hdr);
	fclose(fp);

	fp = test_open_read("large-mtime/pax.tar");
	TEST_ASSERT(read_header(fp, &hdr) == 0);
	TEST_EQUAL_UI(hdr.sb.st_mode, S_IFREG | 0644);
	TEST_EQUAL_UI(hdr.sb.st_uid, 01750);
	TEST_EQUAL_UI(hdr.sb.st_gid, 01750);
	TEST_EQUAL_UI(hdr.sb.st_size, 5);
#if SIZEOF_TIME_T < 8
	TEST_EQUAL_UI(hdr.sb.st_mtime, INT32_MAX);
#else
	TEST_EQUAL_UI(hdr.sb.st_mtime, 8589934592L);
#endif
	TEST_EQUAL_UI(hdr.mtime, 8589934592L);
	TEST_STR_EQUAL(hdr.name, "input.txt");
	TEST_ASSERT(!hdr.unknown_record);
	TEST_ASSERT(read_retry("data2", fp, buffer, 5) == 0);
	buffer[5] = '\0';
	TEST_STR_EQUAL(buffer, "test\n");
	clear_header(&hdr);
	fclose(fp);

	fp = test_open_read("negative-mtime/pax.tar");
	TEST_ASSERT(read_header(fp, &hdr) == 0);
	TEST_EQUAL_UI(hdr.sb.st_mode, S_IFREG | 0644);
	TEST_EQUAL_UI(hdr.sb.st_uid, 01750);
	TEST_EQUAL_UI(hdr.sb.st_gid, 01750);
	TEST_EQUAL_UI(hdr.sb.st_size, 5);
	TEST_EQUAL_I(hdr.sb.st_mtime, -315622800);
	TEST_EQUAL_UI(hdr.mtime, -315622800);
	TEST_STR_EQUAL(hdr.name, "input.txt");
	TEST_ASSERT(!hdr.unknown_record);
	TEST_ASSERT(read_retry("data3", fp, buffer, 5) == 0);
	buffer[5] = '\0';
	TEST_STR_EQUAL(buffer, "test\n");
	clear_header(&hdr);
	fclose(fp);

	fp = test_open_read("long-paths/pax.tar");
	TEST_ASSERT(read_header(fp, &hdr) == 0);
	TEST_EQUAL_UI(hdr.sb.st_mode, S_IFREG | 0644);
	TEST_EQUAL_UI(hdr.sb.st_uid, 01750);
	TEST_EQUAL_UI(hdr.sb.st_gid, 01750);
	TEST_EQUAL_UI(hdr.sb.st_size, 5);
	TEST_EQUAL_UI(hdr.sb.st_mtime, 1542909670);
	TEST_EQUAL_UI(hdr.mtime, 1542909670);
	TEST_STR_EQUAL(hdr.name, filename);
	TEST_ASSERT(!hdr.unknown_record);
	TEST_ASSERT(read_retry("data4", fp, buffer, 5) == 0);
	buffer[5] = '\0';
	TEST_STR_EQUAL(buffer, "test\n");
	clear_header(&hdr);
	fclose(fp);

	return EXIT_SUCCESS;
}
