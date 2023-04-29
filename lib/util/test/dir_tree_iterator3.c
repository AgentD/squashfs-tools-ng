/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * dir_tree_iterator2.c
 *
 * Copyright (C) 2023 David Oberhollenzer <goliath@infraroot.at>
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

	/********** match name **********/
	memset(&cfg, 0, sizeof(cfg));
	cfg.name_pattern = "file_x*";

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

	TEST_STR_EQUAL(ent[0]->name, "dirb/dirx/file_x0");
	TEST_ASSERT(S_ISREG(ent[0]->mode));
	TEST_STR_EQUAL(ent[1]->name, "dirb/dirx/file_x1");
	TEST_ASSERT(S_ISREG(ent[1]->mode));
	TEST_STR_EQUAL(ent[2]->name, "dirb/dirx/file_x2");
	TEST_ASSERT(S_ISREG(ent[2]->mode));

	for (i = 0; i < 3; ++i)
		free(ent[i]);

	/********** match path **********/
	memset(&cfg, 0, sizeof(cfg));
	cfg.name_pattern = "dir*/file_*0";
	cfg.flags |= DIR_SCAN_MATCH_FULL_PATH;

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

	TEST_STR_EQUAL(ent[0]->name, "dira/file_a0");
	TEST_ASSERT(S_ISREG(ent[0]->mode));
	TEST_STR_EQUAL(ent[1]->name, "dirb/file_b0");
	TEST_ASSERT(S_ISREG(ent[1]->mode));
	TEST_STR_EQUAL(ent[2]->name, "dirc/file_c0");
	TEST_ASSERT(S_ISREG(ent[2]->mode));

	for (i = 0; i < 3; ++i)
		free(ent[i]);

	return EXIT_SUCCESS;
}
