/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * tar_iterator.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "tar/tar.h"
#include "util/test.h"
#include "sqfs/error.h"
#include "sqfs/io.h"
#include "sqfs/dir_entry.h"

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
	sqfs_istream_t *fp, *ti, *ti2;
	sqfs_dir_iterator_t *it;
	sqfs_dir_entry_t *ent;
	char buffer[100];
	sqfs_s32 ret;
	sqfs_s64 ts;
	int iret;
	(void)argc; (void)argv;

	/* Open the file, create an iterator */
	iret = sqfs_istream_open_file(&fp,
				      STRVALUE(TESTPATH) "/" STRVALUE(TESTFILE),
				      0);
	TEST_EQUAL_I(iret, 0);
	TEST_NOT_NULL(fp);
	TEST_EQUAL_UI(((sqfs_object_t *)fp)->refcount, 1);
	it = tar_open_stream(fp, NULL);
	TEST_NOT_NULL(it);
	TEST_EQUAL_UI(((sqfs_object_t *)fp)->refcount, 2);
	TEST_EQUAL_UI(((sqfs_object_t *)it)->refcount, 1);

	/* read entry */
	ret = it->next(it, &ent);
	TEST_EQUAL_I(ret, 0);
	TEST_NOT_NULL(ent);
	TEST_EQUAL_UI(((sqfs_object_t *)fp)->refcount, 2);
	TEST_EQUAL_UI(((sqfs_object_t *)it)->refcount, 1);

	ts = TESTTS;
	TEST_EQUAL_UI(ent->mode, S_IFREG | 0644);
	TEST_EQUAL_UI(ent->uid, TESTUID);
	TEST_EQUAL_UI(ent->gid, TESTGID);
	TEST_EQUAL_UI(ent->mtime, ts);
	TEST_STR_EQUAL(ent->name, fname);
	free(ent);
	ent = NULL;

	/* Open file stream */
	ret = it->open_file_ro(it, &ti);
	TEST_EQUAL_I(ret, 0);
	TEST_NOT_NULL(ti);
	TEST_EQUAL_UI(((sqfs_object_t *)fp)->refcount, 2);
	TEST_EQUAL_UI(((sqfs_object_t *)it)->refcount, 2);
	TEST_EQUAL_UI(((sqfs_object_t *)ti)->refcount, 1);

	/* test that the iterator is now "locked" */
	ret = it->open_file_ro(it, &ti2);
	TEST_EQUAL_I(ret, SQFS_ERROR_SEQUENCE);
	TEST_NULL(ti2);
	ret = it->next(it, &ent);
	TEST_EQUAL_I(ret, SQFS_ERROR_SEQUENCE);
	TEST_NULL(ent);
	TEST_EQUAL_UI(((sqfs_object_t *)fp)->refcount, 2);
	TEST_EQUAL_UI(((sqfs_object_t *)it)->refcount, 2);
	TEST_EQUAL_UI(((sqfs_object_t *)ti)->refcount, 1);

	/* read the data from the stream */
	ret = sqfs_istream_read(ti, buffer, sizeof(buffer));
	TEST_EQUAL_I(ret, 5);
	buffer[5] = '\0';
	TEST_STR_EQUAL(buffer, "test\n");

	ret = sqfs_istream_read(ti, buffer, sizeof(buffer));
	TEST_EQUAL_I(ret, 0);

	sqfs_drop(ti);
	TEST_EQUAL_UI(((sqfs_object_t *)fp)->refcount, 2);
	TEST_EQUAL_UI(((sqfs_object_t *)it)->refcount, 1);

	/* read past EOF on the iterator */
	ret = it->next(it, &ent);
	TEST_ASSERT(ret > 0);
	TEST_NULL(ent);

	/* cleanup */
	sqfs_drop(it);
	TEST_EQUAL_UI(((sqfs_object_t *)fp)->refcount, 1);
	sqfs_drop(fp);

	/* re-open the tar iterator */
	iret = sqfs_istream_open_file(&fp,
				      STRVALUE(TESTPATH) "/" STRVALUE(TESTFILE),
				      0);
	TEST_EQUAL_I(iret, 0);
	TEST_NOT_NULL(fp);
	it = tar_open_stream(fp, NULL);
	TEST_NOT_NULL(it);

	/* read entry */
	ret = it->next(it, &ent);
	TEST_EQUAL_I(ret, 0);
	TEST_NOT_NULL(ent);

	ts = TESTTS;
	TEST_EQUAL_UI(ent->mode, S_IFREG | 0644);
	TEST_EQUAL_UI(ent->uid, TESTUID);
	TEST_EQUAL_UI(ent->gid, TESTGID);
	TEST_EQUAL_UI(ent->mtime, ts);
	TEST_STR_EQUAL(ent->name, fname);
	free(ent);
	ent = NULL;

	/* read next entry, mus treturn EOF */
	ret = it->next(it, &ent);
	TEST_ASSERT(ret > 0);
	TEST_NULL(ent);

	/* cleanup */
	sqfs_drop(it);
	TEST_EQUAL_UI(((sqfs_object_t *)fp)->refcount, 1);
	sqfs_drop(fp);

	return EXIT_SUCCESS;
}
