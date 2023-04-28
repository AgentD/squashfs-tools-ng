/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * dir_tree_iterator.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "util/dir_tree_iterator.h"
#include "sqfs/error.h"
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
	dir_entry_t *ent[17];
	dir_iterator_t *dir;
	dir_tree_cfg_t cfg;
	size_t i;
	int ret;
	(void)argc; (void)argv;

	memset(&cfg, 0, sizeof(cfg));

	dir = dir_tree_iterator_create(TEST_PATH, &cfg);
	TEST_NOT_NULL(dir);

	for (i = 0; i < 16; ++i) {
		ret = dir->next(dir, &ent[i]);
		TEST_NOT_NULL(ent[i]);
		TEST_EQUAL_I(ret, 0);
		printf("READ %s\n", ent[i]->name);
	}

	ret = dir->next(dir, &ent[16]);
	TEST_NULL(ent[16]);
	TEST_ASSERT(ret > 0);

	dir = sqfs_drop(dir);

	qsort(ent, 16, sizeof(ent[0]), compare_entries);

	printf("After sort:\n");
	for (i = 0; i < 16; ++i)
		printf("%s\n", ent[i]->name);

	TEST_STR_EQUAL(ent[0]->name, "dira");
	TEST_ASSERT(S_ISDIR(ent[0]->mode));
	TEST_STR_EQUAL(ent[1]->name, "dira/file_a0");
	TEST_ASSERT(S_ISREG(ent[1]->mode));
	TEST_STR_EQUAL(ent[2]->name, "dira/file_a1");
	TEST_ASSERT(S_ISREG(ent[2]->mode));
	TEST_STR_EQUAL(ent[3]->name, "dira/file_a2");
	TEST_ASSERT(S_ISREG(ent[3]->mode));
	TEST_STR_EQUAL(ent[4]->name, "dirb");
	TEST_ASSERT(S_ISDIR(ent[4]->mode));
	TEST_STR_EQUAL(ent[5]->name, "dirb/dirx");
	TEST_ASSERT(S_ISDIR(ent[5]->mode));
	TEST_STR_EQUAL(ent[6]->name, "dirb/dirx/file_x0");
	TEST_ASSERT(S_ISREG(ent[6]->mode));
	TEST_STR_EQUAL(ent[7]->name, "dirb/dirx/file_x1");
	TEST_ASSERT(S_ISREG(ent[7]->mode));
	TEST_STR_EQUAL(ent[8]->name, "dirb/dirx/file_x2");
	TEST_ASSERT(S_ISREG(ent[8]->mode));
	TEST_STR_EQUAL(ent[9]->name, "dirb/file_b0");
	TEST_ASSERT(S_ISREG(ent[9]->mode));
	TEST_STR_EQUAL(ent[10]->name, "dirb/file_b1");
	TEST_ASSERT(S_ISREG(ent[10]->mode));
	TEST_STR_EQUAL(ent[11]->name, "dirb/file_b2");
	TEST_ASSERT(S_ISREG(ent[11]->mode));
	TEST_STR_EQUAL(ent[12]->name, "dirc");
	TEST_ASSERT(S_ISDIR(ent[12]->mode));
	TEST_STR_EQUAL(ent[13]->name, "dirc/file_c0");
	TEST_ASSERT(S_ISREG(ent[13]->mode));
	TEST_STR_EQUAL(ent[14]->name, "dirc/file_c1");
	TEST_ASSERT(S_ISREG(ent[14]->mode));
	TEST_STR_EQUAL(ent[15]->name, "dirc/file_c2");
	TEST_ASSERT(S_ISREG(ent[15]->mode));

	for (i = 0; i < 16; ++i)
		free(ent[i]);

	/* retry with skipping */
	printf("**********\n");

	dir = dir_tree_iterator_create(TEST_PATH, &cfg);
	TEST_NOT_NULL(dir);

	for (i = 0; i < 13; ++i) {
		ret = dir->next(dir, &ent[i]);
		TEST_NOT_NULL(ent[i]);
		TEST_EQUAL_I(ret, 0);
		printf("READ %s\n", ent[i]->name);

		if (!strcmp(ent[i]->name, "dirb/dirx"))
			dir_tree_iterator_skip(dir);
	}

	ret = dir->next(dir, &ent[13]);
	TEST_NULL(ent[13]);
	TEST_ASSERT(ret > 0);

	dir = sqfs_drop(dir);

	qsort(ent, 13, sizeof(ent[0]), compare_entries);

	printf("After sort:\n");
	for (i = 0; i < 13; ++i)
		printf("%s\n", ent[i]->name);

	TEST_STR_EQUAL(ent[0]->name, "dira");
	TEST_ASSERT(S_ISDIR(ent[0]->mode));
	TEST_STR_EQUAL(ent[1]->name, "dira/file_a0");
	TEST_ASSERT(S_ISREG(ent[1]->mode));
	TEST_STR_EQUAL(ent[2]->name, "dira/file_a1");
	TEST_ASSERT(S_ISREG(ent[2]->mode));
	TEST_STR_EQUAL(ent[3]->name, "dira/file_a2");
	TEST_ASSERT(S_ISREG(ent[3]->mode));
	TEST_STR_EQUAL(ent[4]->name, "dirb");
	TEST_ASSERT(S_ISDIR(ent[4]->mode));
	TEST_STR_EQUAL(ent[5]->name, "dirb/dirx");
	TEST_ASSERT(S_ISDIR(ent[5]->mode));
	TEST_STR_EQUAL(ent[6]->name, "dirb/file_b0");
	TEST_ASSERT(S_ISREG(ent[6]->mode));
	TEST_STR_EQUAL(ent[7]->name, "dirb/file_b1");
	TEST_ASSERT(S_ISREG(ent[7]->mode));
	TEST_STR_EQUAL(ent[8]->name, "dirb/file_b2");
	TEST_ASSERT(S_ISREG(ent[8]->mode));
	TEST_STR_EQUAL(ent[9]->name, "dirc");
	TEST_ASSERT(S_ISDIR(ent[9]->mode));
	TEST_STR_EQUAL(ent[10]->name, "dirc/file_c0");
	TEST_ASSERT(S_ISREG(ent[10]->mode));
	TEST_STR_EQUAL(ent[11]->name, "dirc/file_c1");
	TEST_ASSERT(S_ISREG(ent[11]->mode));
	TEST_STR_EQUAL(ent[12]->name, "dirc/file_c2");
	TEST_ASSERT(S_ISREG(ent[12]->mode));

	for (i = 0; i < 13; ++i)
		free(ent[i]);

	/* retry with skipping */
	printf("**********\n");

	dir = dir_tree_iterator_create(TEST_PATH, &cfg);
	TEST_NOT_NULL(dir);

	for (i = 0; i < 9; ++i) {
		ret = dir->next(dir, &ent[i]);
		TEST_NOT_NULL(ent[i]);
		TEST_EQUAL_I(ret, 0);
		printf("READ %s\n", ent[i]->name);

		if (!strcmp(ent[i]->name, "dirb"))
			dir_tree_iterator_skip(dir);
	}

	ret = dir->next(dir, &ent[9]);
	TEST_NULL(ent[9]);
	TEST_ASSERT(ret > 0);

	dir = sqfs_drop(dir);

	qsort(ent, 9, sizeof(ent[0]), compare_entries);

	printf("After sort:\n");
	for (i = 0; i < 9; ++i)
		printf("%s\n", ent[i]->name);

	TEST_STR_EQUAL(ent[0]->name, "dira");
	TEST_ASSERT(S_ISDIR(ent[0]->mode));
	TEST_STR_EQUAL(ent[1]->name, "dira/file_a0");
	TEST_ASSERT(S_ISREG(ent[1]->mode));
	TEST_STR_EQUAL(ent[2]->name, "dira/file_a1");
	TEST_ASSERT(S_ISREG(ent[2]->mode));
	TEST_STR_EQUAL(ent[3]->name, "dira/file_a2");
	TEST_ASSERT(S_ISREG(ent[3]->mode));
	TEST_STR_EQUAL(ent[4]->name, "dirb");
	TEST_ASSERT(S_ISDIR(ent[4]->mode));
	TEST_STR_EQUAL(ent[5]->name, "dirc");
	TEST_ASSERT(S_ISDIR(ent[5]->mode));
	TEST_STR_EQUAL(ent[6]->name, "dirc/file_c0");
	TEST_ASSERT(S_ISREG(ent[6]->mode));
	TEST_STR_EQUAL(ent[7]->name, "dirc/file_c1");
	TEST_ASSERT(S_ISREG(ent[7]->mode));
	TEST_STR_EQUAL(ent[8]->name, "dirc/file_c2");
	TEST_ASSERT(S_ISREG(ent[8]->mode));

	for (i = 0; i < 9; ++i)
		free(ent[i]);

	return EXIT_SUCCESS;
}
