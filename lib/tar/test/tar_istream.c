/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * tar_istream.c
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

static const char *fname = STRVALUE(TESTFNAME);

int main(int argc, char **argv)
{
	tar_header_decoded_t hdr;
	char buffer[100];
	sqfs_s64 ts;
	istream_t *fp;
	istream_t *ti;
	sqfs_s32 ret;
	(void)argc; (void)argv;

	fp = istream_open_file(STRVALUE(TESTPATH) "/" STRVALUE(TESTFILE));
	TEST_NOT_NULL(fp);
	TEST_EQUAL_UI(((sqfs_object_t *)fp)->refcount, 1);
	TEST_ASSERT(read_header(fp, &hdr) == 0);
	TEST_EQUAL_UI(((sqfs_object_t *)fp)->refcount, 1);
	TEST_EQUAL_UI(hdr.mode, S_IFREG | 0644);
	TEST_EQUAL_UI(hdr.uid, TESTUID);
	TEST_EQUAL_UI(hdr.gid, TESTGID);
	TEST_EQUAL_UI(hdr.actual_size, 5);

	ts = TESTTS;
	TEST_EQUAL_UI(hdr.mtime, ts);
	TEST_STR_EQUAL(hdr.name, fname);
	TEST_ASSERT(!hdr.unknown_record);

	ti = tar_record_istream_create(fp, &hdr);
	TEST_NOT_NULL(ti);
	TEST_EQUAL_UI(((sqfs_object_t *)fp)->refcount, 2);
	TEST_EQUAL_UI(((sqfs_object_t *)ti)->refcount, 1);

	ret = istream_read(ti, buffer, sizeof(buffer));
	TEST_EQUAL_I(ret, 5);
	buffer[5] = '\0';
	TEST_STR_EQUAL(buffer, "test\n");

	ret = istream_read(ti, buffer, sizeof(buffer));
	TEST_EQUAL_I(ret, 0);

	clear_header(&hdr);
	sqfs_drop(ti);
	TEST_EQUAL_UI(((sqfs_object_t *)fp)->refcount, 1);
	sqfs_drop(fp);
	return EXIT_SUCCESS;
}
