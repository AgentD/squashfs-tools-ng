/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * dir_tree_iterator.c
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

int main(int argc, char **argv)
{
	sqfs_dir_entry_t *ent[17];
	sqfs_dir_iterator_t *dir;
	dir_tree_cfg_t cfg;
	size_t i;
	int ret;
	(void)argc; (void)argv;

	memset(&cfg, 0, sizeof(cfg));
	cfg.def_mtime = 1337;
	cfg.def_uid = 42;
	cfg.def_gid = 23;

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
	TEST_EQUAL_UI(ent[0]->mtime, 1337);
	TEST_EQUAL_UI(ent[0]->uid, 42);
	TEST_EQUAL_UI(ent[0]->gid, 23);
	TEST_STR_EQUAL(ent[1]->name, "dira/file_a0");
	TEST_ASSERT(S_ISREG(ent[1]->mode));
	TEST_EQUAL_UI(ent[1]->mtime, 1337);
	TEST_EQUAL_UI(ent[1]->uid, 42);
	TEST_EQUAL_UI(ent[1]->gid, 23);
	TEST_STR_EQUAL(ent[2]->name, "dira/file_a1");
	TEST_ASSERT(S_ISREG(ent[2]->mode));
	TEST_EQUAL_UI(ent[2]->mtime, 1337);
	TEST_EQUAL_UI(ent[2]->uid, 42);
	TEST_EQUAL_UI(ent[2]->gid, 23);
	TEST_STR_EQUAL(ent[3]->name, "dira/file_a2");
	TEST_ASSERT(S_ISREG(ent[3]->mode));
	TEST_EQUAL_UI(ent[3]->mtime, 1337);
	TEST_EQUAL_UI(ent[3]->uid, 42);
	TEST_EQUAL_UI(ent[3]->gid, 23);
	TEST_STR_EQUAL(ent[4]->name, "dirb");
	TEST_ASSERT(S_ISDIR(ent[4]->mode));
	TEST_EQUAL_UI(ent[4]->mtime, 1337);
	TEST_EQUAL_UI(ent[4]->uid, 42);
	TEST_EQUAL_UI(ent[4]->gid, 23);
	TEST_STR_EQUAL(ent[5]->name, "dirb/dirx");
	TEST_ASSERT(S_ISDIR(ent[5]->mode));
	TEST_EQUAL_UI(ent[5]->mtime, 1337);
	TEST_EQUAL_UI(ent[5]->uid, 42);
	TEST_EQUAL_UI(ent[5]->gid, 23);
	TEST_STR_EQUAL(ent[6]->name, "dirb/dirx/file_x0");
	TEST_ASSERT(S_ISREG(ent[6]->mode));
	TEST_EQUAL_UI(ent[6]->mtime, 1337);
	TEST_EQUAL_UI(ent[6]->uid, 42);
	TEST_EQUAL_UI(ent[6]->gid, 23);
	TEST_STR_EQUAL(ent[7]->name, "dirb/dirx/file_x1");
	TEST_ASSERT(S_ISREG(ent[7]->mode));
	TEST_EQUAL_UI(ent[7]->mtime, 1337);
	TEST_EQUAL_UI(ent[7]->uid, 42);
	TEST_EQUAL_UI(ent[7]->gid, 23);
	TEST_STR_EQUAL(ent[8]->name, "dirb/dirx/file_x2");
	TEST_ASSERT(S_ISREG(ent[8]->mode));
	TEST_EQUAL_UI(ent[8]->mtime, 1337);
	TEST_EQUAL_UI(ent[8]->uid, 42);
	TEST_EQUAL_UI(ent[8]->gid, 23);
	TEST_STR_EQUAL(ent[9]->name, "dirb/file_b0");
	TEST_ASSERT(S_ISREG(ent[9]->mode));
	TEST_EQUAL_UI(ent[9]->mtime, 1337);
	TEST_EQUAL_UI(ent[9]->uid, 42);
	TEST_EQUAL_UI(ent[9]->gid, 23);
	TEST_STR_EQUAL(ent[10]->name, "dirb/file_b1");
	TEST_ASSERT(S_ISREG(ent[10]->mode));
	TEST_EQUAL_UI(ent[10]->mtime, 1337);
	TEST_EQUAL_UI(ent[10]->uid, 42);
	TEST_EQUAL_UI(ent[10]->gid, 23);
	TEST_STR_EQUAL(ent[11]->name, "dirb/file_b2");
	TEST_ASSERT(S_ISREG(ent[11]->mode));
	TEST_EQUAL_UI(ent[11]->mtime, 1337);
	TEST_EQUAL_UI(ent[11]->uid, 42);
	TEST_EQUAL_UI(ent[11]->gid, 23);
	TEST_STR_EQUAL(ent[12]->name, "dirc");
	TEST_ASSERT(S_ISDIR(ent[12]->mode));
	TEST_EQUAL_UI(ent[12]->mtime, 1337);
	TEST_EQUAL_UI(ent[12]->uid, 42);
	TEST_EQUAL_UI(ent[12]->gid, 23);
	TEST_STR_EQUAL(ent[13]->name, "dirc/file_c0");
	TEST_ASSERT(S_ISREG(ent[13]->mode));
	TEST_EQUAL_UI(ent[13]->mtime, 1337);
	TEST_EQUAL_UI(ent[13]->uid, 42);
	TEST_EQUAL_UI(ent[13]->gid, 23);
	TEST_STR_EQUAL(ent[14]->name, "dirc/file_c1");
	TEST_ASSERT(S_ISREG(ent[14]->mode));
	TEST_EQUAL_UI(ent[14]->mtime, 1337);
	TEST_EQUAL_UI(ent[14]->uid, 42);
	TEST_EQUAL_UI(ent[14]->gid, 23);
	TEST_STR_EQUAL(ent[15]->name, "dirc/file_c2");
	TEST_ASSERT(S_ISREG(ent[15]->mode));
	TEST_EQUAL_UI(ent[15]->mtime, 1337);
	TEST_EQUAL_UI(ent[15]->uid, 42);
	TEST_EQUAL_UI(ent[15]->gid, 23);

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
			dir->ignore_subdir(dir);
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
			dir->ignore_subdir(dir);
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
