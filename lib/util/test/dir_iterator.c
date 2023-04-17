/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * dir_iterator.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "util/dir_iterator.h"
#include "util/test.h"
#include "compat.h"

static int compare_entries(const void *a, const void *b)
{
	const dir_entry_t *const *lhs = a;
	const dir_entry_t *const *rhs = b;

	return strcmp((*lhs)->name, (*rhs)->name);
}

int main(int argc, char **argv)
{
	dir_iterator_t *dir;
	dir_entry_t *ent[6];
	size_t i;
	int ret;
	(void)argc; (void)argv;

	/* scan the top level hierarchy */
	dir = dir_iterator_create(TEST_PATH);
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
	dir = dir_iterator_create(TEST_PATH "/dira");
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
	dir = dir_iterator_create(TEST_PATH "/dirb");
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
	TEST_STR_EQUAL(ent[2]->name, "file_b0");
	TEST_ASSERT(S_ISREG(ent[2]->mode));
	TEST_STR_EQUAL(ent[3]->name, "file_b1");
	TEST_ASSERT(S_ISREG(ent[3]->mode));
	TEST_STR_EQUAL(ent[4]->name, "file_b2");
	TEST_ASSERT(S_ISREG(ent[4]->mode));

	for (i = 0; i < 5; ++i)
		free(ent[i]);

	/* scan first sub hierarchy */
	dir = dir_iterator_create(TEST_PATH "/dirc");
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

	return EXIT_SUCCESS;
}
