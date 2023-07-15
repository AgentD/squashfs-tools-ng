/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * dir_iterator.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "io/dir_iterator.h"
#include "sqfs/error.h"
#include "util/test.h"
#include "sqfs/io.h"
#include "compat.h"

static int compare_entries(const void *a, const void *b)
{
	const sqfs_dir_entry_t *const *lhs = a;
	const sqfs_dir_entry_t *const *rhs = b;

	return strcmp((*lhs)->name, (*rhs)->name);
}

static int compare_files(const void *a, const void *b)
{
	const sqfs_istream_t *const *lhs = a;
	const sqfs_istream_t *const *rhs = b;

	return strcmp((*lhs)->get_filename(*lhs),
		      (*rhs)->get_filename(*rhs));
}

int main(int argc, char **argv)
{
	sqfs_dir_iterator_t *dir, *suba, *subb, *subc, *sub;
	sqfs_dir_entry_t *dent, *ent[6];
	sqfs_istream_t *files[3];
	char buffer[128];
	size_t i, j;
	int ret;
	(void)argc; (void)argv;

	/* scan the top level hierarchy */
	ret = sqfs_dir_iterator_create_native(&dir, TEST_PATH, 0);
	TEST_EQUAL_I(ret, 0);
	TEST_NOT_NULL(dir);

	ret = dir->next(dir, &ent[0]);
	TEST_NOT_NULL(ent[0]);
	TEST_EQUAL_I(ret, 0);

	ret = dir->next(dir, &ent[1]);
	TEST_NOT_NULL(ent[1]);
	TEST_EQUAL_I(ret, 0);

	ret = dir->next(dir, &ent[2]);
	TEST_NOT_NULL(ent[2]);
	TEST_EQUAL_I(ret, 0);

	ret = dir->next(dir, &ent[3]);
	TEST_NOT_NULL(ent[3]);
	TEST_EQUAL_I(ret, 0);

	ret = dir->next(dir, &ent[4]);
	TEST_NOT_NULL(ent[4]);
	TEST_EQUAL_I(ret, 0);

	ret = dir->next(dir, &ent[5]);
	TEST_NULL(ent[5]);
	TEST_ASSERT(ret > 0);

	dir = sqfs_drop(dir);

	qsort(ent, 5, sizeof(ent[0]), compare_entries);

	TEST_STR_EQUAL(ent[0]->name, ".");
	TEST_ASSERT(S_ISDIR(ent[0]->mode));
	TEST_STR_EQUAL(ent[1]->name, "..");
	TEST_ASSERT(S_ISDIR(ent[1]->mode));
	TEST_STR_EQUAL(ent[2]->name, "dira");
	TEST_ASSERT(S_ISDIR(ent[2]->mode));
	TEST_STR_EQUAL(ent[3]->name, "dirb");
	TEST_ASSERT(S_ISDIR(ent[3]->mode));
	TEST_STR_EQUAL(ent[4]->name, "dirc");
	TEST_ASSERT(S_ISDIR(ent[4]->mode));

	for (i = 0; i < 5; ++i)
		free(ent[i]);

	/* scan first sub hierarchy */
	ret = sqfs_dir_iterator_create_native(&dir, TEST_PATH "/dira", 0);
	TEST_EQUAL_I(ret, 0);
	TEST_NOT_NULL(dir);

	ret = dir->next(dir, &ent[0]);
	TEST_NOT_NULL(ent[0]);
	TEST_EQUAL_I(ret, 0);

	ret = dir->next(dir, &ent[1]);
	TEST_NOT_NULL(ent[1]);
	TEST_EQUAL_I(ret, 0);

	ret = dir->next(dir, &ent[2]);
	TEST_NOT_NULL(ent[2]);
	TEST_EQUAL_I(ret, 0);

	ret = dir->next(dir, &ent[3]);
	TEST_NOT_NULL(ent[3]);
	TEST_EQUAL_I(ret, 0);

	ret = dir->next(dir, &ent[4]);
	TEST_NOT_NULL(ent[4]);
	TEST_EQUAL_I(ret, 0);

	ret = dir->next(dir, &ent[5]);
	TEST_NULL(ent[5]);
	TEST_ASSERT(ret > 0);

	dir = sqfs_drop(dir);

	qsort(ent, 5, sizeof(ent[0]), compare_entries);

	TEST_STR_EQUAL(ent[0]->name, ".");
	TEST_ASSERT(S_ISDIR(ent[0]->mode));
	TEST_STR_EQUAL(ent[1]->name, "..");
	TEST_ASSERT(S_ISDIR(ent[1]->mode));
	TEST_STR_EQUAL(ent[2]->name, "file_a0");
	TEST_ASSERT(S_ISREG(ent[2]->mode));
	TEST_STR_EQUAL(ent[3]->name, "file_a1");
	TEST_ASSERT(S_ISREG(ent[3]->mode));
	TEST_STR_EQUAL(ent[4]->name, "file_a2");
	TEST_ASSERT(S_ISREG(ent[4]->mode));

	for (i = 0; i < 5; ++i)
		free(ent[i]);

	/* scan second sub hierarchy */
	ret = sqfs_dir_iterator_create_native(&dir, TEST_PATH "/dirb", 0);
	TEST_EQUAL_I(ret, 0);
	TEST_NOT_NULL(dir);

	ret = dir->next(dir, &ent[0]);
	TEST_NOT_NULL(ent[0]);
	TEST_EQUAL_I(ret, 0);

	ret = dir->next(dir, &ent[1]);
	TEST_NOT_NULL(ent[1]);
	TEST_EQUAL_I(ret, 0);

	ret = dir->next(dir, &ent[2]);
	TEST_NOT_NULL(ent[2]);
	TEST_EQUAL_I(ret, 0);

	ret = dir->next(dir, &ent[3]);
	TEST_NOT_NULL(ent[3]);
	TEST_EQUAL_I(ret, 0);

	ret = dir->next(dir, &ent[4]);
	TEST_NOT_NULL(ent[4]);
	TEST_EQUAL_I(ret, 0);

	ret = dir->next(dir, &ent[5]);
	TEST_NOT_NULL(ent[5]);
	TEST_EQUAL_I(ret, 0);

	ret = dir->next(dir, &dent);
	TEST_NULL(dent);
	TEST_ASSERT(ret > 0);

	dir = sqfs_drop(dir);

	qsort(ent, 6, sizeof(ent[0]), compare_entries);

	TEST_STR_EQUAL(ent[0]->name, ".");
	TEST_ASSERT(S_ISDIR(ent[0]->mode));
	TEST_STR_EQUAL(ent[1]->name, "..");
	TEST_ASSERT(S_ISDIR(ent[1]->mode));
	TEST_STR_EQUAL(ent[2]->name, "dirx");
	TEST_ASSERT(S_ISDIR(ent[2]->mode));
	TEST_STR_EQUAL(ent[3]->name, "file_b0");
	TEST_ASSERT(S_ISREG(ent[3]->mode));
	TEST_STR_EQUAL(ent[4]->name, "file_b1");
	TEST_ASSERT(S_ISREG(ent[4]->mode));
	TEST_STR_EQUAL(ent[5]->name, "file_b2");
	TEST_ASSERT(S_ISREG(ent[5]->mode));

	for (i = 0; i < 6; ++i)
		free(ent[i]);

	/* scan first sub hierarchy */
	ret = sqfs_dir_iterator_create_native(&dir, TEST_PATH "/dirc", 0);
	TEST_EQUAL_I(ret, 0);
	TEST_NOT_NULL(dir);

	ret = dir->next(dir, &ent[0]);
	TEST_NOT_NULL(ent[0]);
	TEST_EQUAL_I(ret, 0);

	ret = dir->next(dir, &ent[1]);
	TEST_NOT_NULL(ent[1]);
	TEST_EQUAL_I(ret, 0);

	ret = dir->next(dir, &ent[2]);
	TEST_NOT_NULL(ent[2]);
	TEST_EQUAL_I(ret, 0);

	ret = dir->next(dir, &ent[3]);
	TEST_NOT_NULL(ent[3]);
	TEST_EQUAL_I(ret, 0);

	ret = dir->next(dir, &ent[4]);
	TEST_NOT_NULL(ent[4]);
	TEST_EQUAL_I(ret, 0);

	ret = dir->next(dir, &ent[5]);
	TEST_NULL(ent[5]);
	TEST_ASSERT(ret > 0);

	dir = sqfs_drop(dir);

	qsort(ent, 5, sizeof(ent[0]), compare_entries);

	TEST_STR_EQUAL(ent[0]->name, ".");
	TEST_ASSERT(S_ISDIR(ent[0]->mode));
	TEST_STR_EQUAL(ent[1]->name, "..");
	TEST_ASSERT(S_ISDIR(ent[1]->mode));
	TEST_STR_EQUAL(ent[2]->name, "file_c0");
	TEST_ASSERT(S_ISREG(ent[2]->mode));
	TEST_STR_EQUAL(ent[3]->name, "file_c1");
	TEST_ASSERT(S_ISREG(ent[3]->mode));
	TEST_STR_EQUAL(ent[4]->name, "file_c2");
	TEST_ASSERT(S_ISREG(ent[4]->mode));

	for (i = 0; i < 5; ++i)
		free(ent[i]);

	/* test sub directory iterators */
	suba = NULL;
	subb = NULL;
	subc = NULL;

	ret = sqfs_dir_iterator_create_native(&dir, TEST_PATH, 0);
	TEST_EQUAL_I(ret, 0);
	TEST_NOT_NULL(dir);

	for (i = 0; i < 5; ++i) {
		ret = dir->next(dir, &dent);
		TEST_NOT_NULL(dent);
		TEST_EQUAL_I(ret, 0);

		if (!strcmp(dent->name, "dira")) {
			TEST_NULL(suba);
			ret = dir->open_subdir(dir, &suba);
			TEST_NOT_NULL(suba);
			TEST_EQUAL_I(ret, 0);
		} else if (!strcmp(dent->name, "dirb")) {
			TEST_NULL(subb);
			ret = dir->open_subdir(dir, &subb);
			TEST_NOT_NULL(subb);
			TEST_EQUAL_I(ret, 0);
		} else if (!strcmp(dent->name, "dirc")) {
			TEST_NULL(subc);
			ret = dir->open_subdir(dir, &subc);
			TEST_NOT_NULL(subc);
			TEST_EQUAL_I(ret, 0);
		}

		free(dent);
	}

	ret = dir->next(dir, &dent);
	TEST_NULL(dent);
	TEST_ASSERT(ret > 0);
	dir = sqfs_drop(dir);

	TEST_NOT_NULL(suba);
	TEST_NOT_NULL(subb);
	TEST_NOT_NULL(subc);

	/* sub iterator a */
	for (i = 0; i < 5; ++i) {
		ret = suba->next(suba, &ent[i]);
		TEST_NOT_NULL(ent[0]);
		TEST_EQUAL_I(ret, 0);

		if (S_ISREG(ent[i]->mode)) {
			ret = suba->open_subdir(suba, &sub);
			TEST_NULL(sub);
			TEST_EQUAL_I(ret, SQFS_ERROR_NOT_DIR);
		}
	}

	ret = suba->next(suba, &dent);
	TEST_NULL(dent);
	TEST_ASSERT(ret > 0);
	suba = sqfs_drop(suba);

	qsort(ent, 5, sizeof(ent[0]), compare_entries);

	TEST_STR_EQUAL(ent[0]->name, ".");
	TEST_ASSERT(S_ISDIR(ent[0]->mode));
	TEST_STR_EQUAL(ent[1]->name, "..");
	TEST_ASSERT(S_ISDIR(ent[1]->mode));
	TEST_STR_EQUAL(ent[2]->name, "file_a0");
	TEST_ASSERT(S_ISREG(ent[2]->mode));
	TEST_STR_EQUAL(ent[3]->name, "file_a1");
	TEST_ASSERT(S_ISREG(ent[3]->mode));
	TEST_STR_EQUAL(ent[4]->name, "file_a2");
	TEST_ASSERT(S_ISREG(ent[4]->mode));

	for (i = 0; i < 5; ++i)
		free(ent[i]);

	/* sub iterator b */
	for (i = 0; i < 6; ++i) {
		ret = subb->next(subb, &ent[i]);
		TEST_NOT_NULL(ent[0]);
		TEST_EQUAL_I(ret, 0);

		if (S_ISREG(ent[i]->mode)) {
			ret = subb->open_subdir(subb, &sub);
			TEST_NULL(sub);
			TEST_EQUAL_I(ret, SQFS_ERROR_NOT_DIR);
		}
	}

	ret = subb->next(subb, &dent);
	TEST_NULL(dent);
	TEST_ASSERT(ret > 0);
	subb = sqfs_drop(subb);

	qsort(ent, 6, sizeof(ent[0]), compare_entries);

	TEST_STR_EQUAL(ent[0]->name, ".");
	TEST_ASSERT(S_ISDIR(ent[0]->mode));
	TEST_STR_EQUAL(ent[1]->name, "..");
	TEST_ASSERT(S_ISDIR(ent[1]->mode));
	TEST_STR_EQUAL(ent[2]->name, "dirx");
	TEST_ASSERT(S_ISDIR(ent[2]->mode));
	TEST_STR_EQUAL(ent[3]->name, "file_b0");
	TEST_ASSERT(S_ISREG(ent[3]->mode));
	TEST_STR_EQUAL(ent[4]->name, "file_b1");
	TEST_ASSERT(S_ISREG(ent[4]->mode));
	TEST_STR_EQUAL(ent[5]->name, "file_b2");
	TEST_ASSERT(S_ISREG(ent[5]->mode));

	for (i = 0; i < 6; ++i)
		free(ent[i]);

	/* sub iterator c */
	j = 0;

	for (i = 0; i < 5; ++i) {
		ret = subc->next(subc, &ent[i]);
		TEST_NOT_NULL(ent[0]);
		TEST_EQUAL_I(ret, 0);

		if (S_ISREG(ent[i]->mode)) {
			ret = subc->open_subdir(subc, &sub);
			TEST_NULL(sub);
			TEST_EQUAL_I(ret, SQFS_ERROR_NOT_DIR);

			TEST_ASSERT(j < 3);
			ret = subc->open_file_ro(subc, &(files[j++]));
			TEST_EQUAL_I(ret, 0);
		}
	}

	TEST_EQUAL_UI(j, 3);

	ret = subc->next(subc, &dent);
	TEST_NULL(dent);
	TEST_ASSERT(ret > 0);
	subc = sqfs_drop(subc);

	qsort(ent, 5, sizeof(ent[0]), compare_entries);
	qsort(files, 3, sizeof(files[0]), compare_files);

	TEST_STR_EQUAL(ent[0]->name, ".");
	TEST_ASSERT(S_ISDIR(ent[0]->mode));
	TEST_STR_EQUAL(ent[1]->name, "..");
	TEST_ASSERT(S_ISDIR(ent[1]->mode));
	TEST_STR_EQUAL(ent[2]->name, "file_c0");
	TEST_ASSERT(S_ISREG(ent[2]->mode));
	TEST_STR_EQUAL(ent[3]->name, "file_c1");
	TEST_ASSERT(S_ISREG(ent[3]->mode));
	TEST_STR_EQUAL(ent[4]->name, "file_c2");
	TEST_ASSERT(S_ISREG(ent[4]->mode));

	for (i = 0; i < 5; ++i)
		free(ent[i]);

	TEST_STR_EQUAL(files[0]->get_filename(files[0]), "file_c0");
	TEST_STR_EQUAL(files[1]->get_filename(files[1]), "file_c1");
	TEST_STR_EQUAL(files[2]->get_filename(files[2]), "file_c2");

	ret = sqfs_istream_read(files[0], buffer, sizeof(buffer));
	TEST_EQUAL_I(ret, 12);
	buffer[ret] = '\0';
	TEST_STR_EQUAL(buffer, "test string\n");
	ret = sqfs_istream_read(files[0], buffer, sizeof(buffer));
	TEST_EQUAL_I(ret, 0);

	ret = sqfs_istream_read(files[1], buffer, sizeof(buffer));
	TEST_EQUAL_I(ret, 0);

	ret = sqfs_istream_read(files[2], buffer, sizeof(buffer));
	TEST_EQUAL_I(ret, 10);
	buffer[ret] = '\0';
	TEST_STR_EQUAL(buffer, "你好！\n");
	ret = sqfs_istream_read(files[2], buffer, sizeof(buffer));
	TEST_EQUAL_I(ret, 0);

	for (i = 0; i < j; ++i)
		sqfs_drop(files[i]);

	return EXIT_SUCCESS;
}
