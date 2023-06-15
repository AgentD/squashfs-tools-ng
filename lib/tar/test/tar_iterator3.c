
/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * tar_iterator3.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "tar/tar.h"
#include "util/test.h"
#include "sqfs/error.h"
#include "sqfs/io.h"

int main(int argc, char **argv)
{
	sqfs_istream_t *fp, *ti;
	dir_iterator_t *it;
	char buffer[100];
	dir_entry_t *ent;
	char *link;
	int ret;
	(void)argc; (void)argv;

	TEST_ASSERT(chdir(TEST_PATH) == 0);

	ret = sqfs_istream_open_file(&fp, "format-acceptance/link_filled.tar");
	TEST_EQUAL_I(ret, 0);
	TEST_NOT_NULL(fp);
	TEST_EQUAL_UI(((sqfs_object_t *)fp)->refcount, 1);
	it = tar_open_stream(fp);
	TEST_NOT_NULL(it);
	TEST_EQUAL_UI(((sqfs_object_t *)fp)->refcount, 2);
	TEST_EQUAL_UI(((sqfs_object_t *)it)->refcount, 1);
	sqfs_drop(fp);

	/* "deep" directory hierarchy containg 2 files */
	ret = it->next(it, &ent);
	TEST_EQUAL_I(ret, 0);
	TEST_NOT_NULL(ent);
	TEST_EQUAL_UI(ent->mode, S_IFDIR | 0777);
	TEST_STR_EQUAL(ent->name, "20_characters_here01");
	free(ent);

	ret = it->read_link(it, &link);
	TEST_EQUAL_I(ret, SQFS_ERROR_NO_ENTRY);
	TEST_NULL(link);
	ret = it->open_file_ro(it, &ti);
	TEST_EQUAL_I(ret, SQFS_ERROR_NOT_FILE);
	TEST_NULL(ti);

	ret = it->next(it, &ent);
	TEST_EQUAL_I(ret, 0);
	TEST_NOT_NULL(ent);
	TEST_EQUAL_UI(ent->mode, S_IFDIR | 0777);
	TEST_STR_EQUAL(ent->name, "20_characters_here01/20_characters_here02");
	free(ent);

	ret = it->next(it, &ent);
	TEST_EQUAL_I(ret, 0);
	TEST_NOT_NULL(ent);
	TEST_EQUAL_UI(ent->mode, S_IFDIR | 0777);
	TEST_STR_EQUAL(ent->name, "20_characters_here01/20_characters_here02/"
		       "20_characters_here03");
	free(ent);

	ret = it->next(it, &ent);
	TEST_EQUAL_I(ret, 0);
	TEST_NOT_NULL(ent);
	TEST_EQUAL_UI(ent->mode, S_IFDIR | 0777);
	TEST_STR_EQUAL(ent->name, "20_characters_here01/20_characters_here02/"
		       "20_characters_here03/20_characters_here04");
	free(ent);

	ret = it->next(it, &ent);
	TEST_EQUAL_I(ret, 0);
	TEST_NOT_NULL(ent);
	TEST_EQUAL_UI(ent->mode, S_IFREG | 0777);
	TEST_STR_EQUAL(ent->name, "20_characters_here01/20_characters_here02/"
		       "20_characters_here03/20_characters_here04/"
		       "errored_file_tst");
	free(ent);

	ret = it->read_link(it, &link);
	TEST_EQUAL_I(ret, SQFS_ERROR_NO_ENTRY);
	TEST_NULL(link);
	ret = it->open_file_ro(it, &ti);
	TEST_EQUAL_I(ret, 0);
	TEST_NOT_NULL(ti);

	TEST_ASSERT(sqfs_istream_read(ti, buffer, sizeof(buffer)) == 5);
	buffer[5] = '\0';
	TEST_STR_EQUAL(buffer, "test\n");
	TEST_ASSERT(sqfs_istream_read(ti, buffer, sizeof(buffer)) == 0);
	ti = sqfs_drop(ti);

	ret = it->next(it, &ent);
	TEST_EQUAL_I(ret, 0);
	TEST_NOT_NULL(ent);
	TEST_EQUAL_UI(ent->mode, S_IFREG | 0777);
	TEST_STR_EQUAL(ent->name, "20_characters_here01/20_characters_here02/"
		       "20_characters_here03/20_characters_here04/"
		       "some_test_file");
	free(ent);

	ret = it->open_file_ro(it, &ti);
	TEST_EQUAL_I(ret, 0);
	TEST_NOT_NULL(ti);

	TEST_ASSERT(sqfs_istream_read(ti, buffer, sizeof(buffer)) == 5);
	buffer[5] = '\0';
	TEST_STR_EQUAL(buffer, "test\n");
	TEST_ASSERT(sqfs_istream_read(ti, buffer, sizeof(buffer)) == 0);
	ti = sqfs_drop(ti);

	/* "deep" directory hierarchy containg a hard link */
	ret = it->next(it, &ent);
	TEST_EQUAL_I(ret, 0);
	TEST_NOT_NULL(ent);
	TEST_EQUAL_UI(ent->mode, S_IFDIR | 0777);
	TEST_STR_EQUAL(ent->name, "20CharsForLnkTest001");
	free(ent);

	ret = it->next(it, &ent);
	TEST_EQUAL_I(ret, 0);
	TEST_NOT_NULL(ent);
	TEST_EQUAL_UI(ent->mode, S_IFDIR | 0777);
	TEST_STR_EQUAL(ent->name, "20CharsForLnkTest001/20CharsForLnkTest002");
	free(ent);

	ret = it->next(it, &ent);
	TEST_EQUAL_I(ret, 0);
	TEST_NOT_NULL(ent);
	TEST_EQUAL_UI(ent->mode, S_IFDIR | 0777);
	TEST_STR_EQUAL(ent->name, "20CharsForLnkTest001/20CharsForLnkTest002/"
		       "20CharsForLnkTest003");
	free(ent);

	ret = it->next(it, &ent);
	TEST_EQUAL_I(ret, 0);
	TEST_NOT_NULL(ent);
	TEST_EQUAL_UI(ent->mode, S_IFDIR | 0777);
	TEST_STR_EQUAL(ent->name, "20CharsForLnkTest001/20CharsForLnkTest002/"
		       "20CharsForLnkTest003/20CharsForLnkTest004");
	free(ent);

	ret = it->next(it, &ent);
	TEST_EQUAL_I(ret, 0);
	TEST_NOT_NULL(ent);
	TEST_STR_EQUAL(ent->name, "20CharsForLnkTest001/20CharsForLnkTest002/"
		       "20CharsForLnkTest003/20CharsForLnkTest004/"
		       "01234567890123456789");
	TEST_EQUAL_UI(ent->mode, S_IFLNK | 0777);
	TEST_ASSERT((ent->flags & DIR_ENTRY_FLAG_HARD_LINK) != 0);
	free(ent);

	ret = it->read_link(it, &link);
	TEST_EQUAL_I(ret, 0);
	TEST_NOT_NULL(link);
	ret = it->open_file_ro(it, &ti);
	TEST_EQUAL_I(ret, SQFS_ERROR_NOT_FILE);
	TEST_NULL(ti);

	TEST_STR_EQUAL(link, "20_characters_here01/"
		       "20_characters_here02/20_characters_here03/"
		       "20_characters_here04/errored_file_tst");
	free(link);

	/* end of file */
	ret = it->next(it, &ent);
	TEST_ASSERT(ret > 0);
	TEST_NULL(ent);
	sqfs_drop(it);
	return EXIT_SUCCESS;
}
