/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * tar_simple.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "io/file.h"
#include "tar/tar.h"
#include "util/test.h"

#ifndef TESTUID
#define TESTUID 1000
#endif

#ifndef TESTGID
#define TESTGID TESTUID
#endif

#ifndef TESTFNAME
#define TESTFNAME input.txt
#endif

#ifndef TESTTS
#define TESTTS 1542905892
#endif

#ifdef LONG_NAME_TEST
static const char *fname =
"012345678901234567890123456789/012345678901234567890123456789/"
"012345678901234567890123456789/012345678901234567890123456789/"
"012345678901234567890123456789/input.txt";
#else
static const char *fname = STRVALUE(TESTFNAME);
#endif

int main(int argc, char **argv)
{
	tar_header_decoded_t hdr;
	sqfs_istream_t *fp;
	char buffer[6];
	sqfs_s64 ts;
	(void)argc; (void)argv;

	fp = istream_open_file(STRVALUE(TESTPATH) "/" STRVALUE(TESTFILE));
	TEST_NOT_NULL(fp);
	TEST_ASSERT(read_header(fp, &hdr) == 0);
	TEST_EQUAL_UI(hdr.mode, S_IFREG | 0644);
	TEST_EQUAL_UI(hdr.uid, TESTUID);
	TEST_EQUAL_UI(hdr.gid, TESTGID);
	TEST_EQUAL_UI(hdr.actual_size, 5);

	ts = TESTTS;
	TEST_EQUAL_UI(hdr.mtime, ts);
	TEST_STR_EQUAL(hdr.name, fname);
	TEST_ASSERT(!hdr.unknown_record);

	TEST_ASSERT(sqfs_istream_read(fp, buffer, 5) == 5);
	buffer[5] = '\0';
	TEST_STR_EQUAL(buffer, "test\n");
	clear_header(&hdr);
	sqfs_drop(fp);
	return EXIT_SUCCESS;
}
