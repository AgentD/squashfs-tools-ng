/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * dir_tree_iterator2.c
 *
 * Copyright (C) 2023 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "io/dir_iterator.h"
#include "sqfs/error.h"
#include "util/test.h"
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

	/********** without files **********/
	memset(&cfg, 0, sizeof(cfg));
	cfg.flags |= DIR_SCAN_NO_FILE;

	dir = dir_tree_iterator_create(TEST_PATH, &cfg);
	TEST_NOT_NULL(dir);

	for (i = 0; i < 4; ++i) {
		ret = dir->next(dir, &ent[i]);
		TEST_NOT_NULL(ent[i]);
		TEST_EQUAL_I(ret, 0);
		printf("READ %s\n", ent[i]->name);
	}

	ret = dir->next(dir, &ent[4]);
	TEST_NULL(ent[4]);
	TEST_ASSERT(ret > 0);

	dir = sqfs_drop(dir);

	qsort(ent, 4, sizeof(ent[0]), compare_entries);

	printf("After sort:\n");
	for (i = 0; i < 4; ++i)
		printf("%s\n", ent[i]->name);

	TEST_STR_EQUAL(ent[0]->name, "dira");
	TEST_ASSERT(S_ISDIR(ent[0]->mode));
	TEST_STR_EQUAL(ent[1]->name, "dirb");
	TEST_ASSERT(S_ISDIR(ent[1]->mode));
	TEST_STR_EQUAL(ent[2]->name, "dirb/dirx");
	TEST_ASSERT(S_ISDIR(ent[2]->mode));
	TEST_STR_EQUAL(ent[3]->name, "dirc");
	TEST_ASSERT(S_ISDIR(ent[3]->mode));

	for (i = 0; i < 4; ++i)
		free(ent[i]);

	/********** recursive but without dirs **********/
	memset(&cfg, 0, sizeof(cfg));
	cfg.flags |= DIR_SCAN_NO_DIR;

	dir = dir_tree_iterator_create(TEST_PATH, &cfg);
	TEST_NOT_NULL(dir);

	for (i = 0; i < 12; ++i) {
		ret = dir->next(dir, &ent[i]);
		TEST_NOT_NULL(ent[i]);
		TEST_EQUAL_I(ret, 0);
		printf("READ %s\n", ent[i]->name);
	}

	ret = dir->next(dir, &ent[12]);
	TEST_NULL(ent[12]);
	TEST_ASSERT(ret > 0);

	dir = sqfs_drop(dir);

	qsort(ent, 12, sizeof(ent[0]), compare_entries);

	printf("After sort:\n");
	for (i = 0; i < 12; ++i)
		printf("%s\n", ent[i]->name);

	TEST_STR_EQUAL(ent[0]->name, "dira/file_a0");
	TEST_ASSERT(S_ISREG(ent[0]->mode));
	TEST_STR_EQUAL(ent[1]->name, "dira/file_a1");
	TEST_ASSERT(S_ISREG(ent[1]->mode));
	TEST_STR_EQUAL(ent[2]->name, "dira/file_a2");
	TEST_ASSERT(S_ISREG(ent[2]->mode));
	TEST_STR_EQUAL(ent[3]->name, "dirb/dirx/file_x0");
	TEST_ASSERT(S_ISREG(ent[3]->mode));
	TEST_STR_EQUAL(ent[4]->name, "dirb/dirx/file_x1");
	TEST_ASSERT(S_ISREG(ent[4]->mode));
	TEST_STR_EQUAL(ent[5]->name, "dirb/dirx/file_x2");
	TEST_ASSERT(S_ISREG(ent[5]->mode));
	TEST_STR_EQUAL(ent[6]->name, "dirb/file_b0");
	TEST_ASSERT(S_ISREG(ent[6]->mode));
	TEST_STR_EQUAL(ent[7]->name, "dirb/file_b1");
	TEST_ASSERT(S_ISREG(ent[7]->mode));
	TEST_STR_EQUAL(ent[8]->name, "dirb/file_b2");
	TEST_ASSERT(S_ISREG(ent[8]->mode));
	TEST_STR_EQUAL(ent[9]->name, "dirc/file_c0");
	TEST_ASSERT(S_ISREG(ent[9]->mode));
	TEST_STR_EQUAL(ent[10]->name, "dirc/file_c1");
	TEST_ASSERT(S_ISREG(ent[10]->mode));
	TEST_STR_EQUAL(ent[11]->name, "dirc/file_c2");
	TEST_ASSERT(S_ISREG(ent[11]->mode));

	for (i = 0; i < 12; ++i)
		free(ent[i]);

	/********** non-recursive **********/
	memset(&cfg, 0, sizeof(cfg));
	cfg.flags |= DIR_SCAN_NO_RECURSION;

	dir = dir_tree_iterator_create(TEST_PATH, &cfg);
	TEST_NOT_NULL(dir);

	for (i = 0; i < 3; ++i) {
		ret = dir->next(dir, &ent[i]);
		TEST_NOT_NULL(ent[i]);
		TEST_EQUAL_I(ret, 0);
		printf("READ %s\n", ent[i]->name);
	}

	ret = dir->next(dir, &ent[3]);
	TEST_NULL(ent[3]);
	TEST_ASSERT(ret > 0);

	dir = sqfs_drop(dir);

	qsort(ent, 3, sizeof(ent[0]), compare_entries);

	printf("After sort:\n");
	for (i = 0; i < 3; ++i)
		printf("%s\n", ent[i]->name);

	TEST_STR_EQUAL(ent[0]->name, "dira");
	TEST_ASSERT(S_ISDIR(ent[0]->mode));
	TEST_STR_EQUAL(ent[1]->name, "dirb");
	TEST_ASSERT(S_ISDIR(ent[1]->mode));
	TEST_STR_EQUAL(ent[2]->name, "dirc");
	TEST_ASSERT(S_ISDIR(ent[2]->mode));

	for (i = 0; i < 3; ++i)
		free(ent[i]);

	/********** with prefix inserted **********/
	memset(&cfg, 0, sizeof(cfg));
	cfg.prefix = "foobar";

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

	TEST_STR_EQUAL(ent[0]->name, "foobar/dira");
	TEST_ASSERT(S_ISDIR(ent[0]->mode));
	TEST_STR_EQUAL(ent[1]->name, "foobar/dira/file_a0");
	TEST_ASSERT(S_ISREG(ent[1]->mode));
	TEST_STR_EQUAL(ent[2]->name, "foobar/dira/file_a1");
	TEST_ASSERT(S_ISREG(ent[2]->mode));
	TEST_STR_EQUAL(ent[3]->name, "foobar/dira/file_a2");
	TEST_ASSERT(S_ISREG(ent[3]->mode));
	TEST_STR_EQUAL(ent[4]->name, "foobar/dirb");
	TEST_ASSERT(S_ISDIR(ent[4]->mode));
	TEST_STR_EQUAL(ent[5]->name, "foobar/dirb/dirx");
	TEST_ASSERT(S_ISDIR(ent[5]->mode));
	TEST_STR_EQUAL(ent[6]->name, "foobar/dirb/dirx/file_x0");
	TEST_ASSERT(S_ISREG(ent[6]->mode));
	TEST_STR_EQUAL(ent[7]->name, "foobar/dirb/dirx/file_x1");
	TEST_ASSERT(S_ISREG(ent[7]->mode));
	TEST_STR_EQUAL(ent[8]->name, "foobar/dirb/dirx/file_x2");
	TEST_ASSERT(S_ISREG(ent[8]->mode));
	TEST_STR_EQUAL(ent[9]->name, "foobar/dirb/file_b0");
	TEST_ASSERT(S_ISREG(ent[9]->mode));
	TEST_STR_EQUAL(ent[10]->name, "foobar/dirb/file_b1");
	TEST_ASSERT(S_ISREG(ent[10]->mode));
	TEST_STR_EQUAL(ent[11]->name, "foobar/dirb/file_b2");
	TEST_ASSERT(S_ISREG(ent[11]->mode));
	TEST_STR_EQUAL(ent[12]->name, "foobar/dirc");
	TEST_ASSERT(S_ISDIR(ent[12]->mode));
	TEST_STR_EQUAL(ent[13]->name, "foobar/dirc/file_c0");
	TEST_ASSERT(S_ISREG(ent[13]->mode));
	TEST_STR_EQUAL(ent[14]->name, "foobar/dirc/file_c1");
	TEST_ASSERT(S_ISREG(ent[14]->mode));
	TEST_STR_EQUAL(ent[15]->name, "foobar/dirc/file_c2");
	TEST_ASSERT(S_ISREG(ent[15]->mode));

	for (i = 0; i < 16; ++i)
		free(ent[i]);

	return EXIT_SUCCESS;
}
