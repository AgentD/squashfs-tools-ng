/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * rec_dir.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "util/test.h"
#include "sqfs/dir_entry.h"
#include "sqfs/inode.h"
#include "sqfs/io.h"
#include "compat.h"

typedef struct {
	sqfs_dir_iterator_t obj;

	bool current_is_dir;
	size_t level;
	size_t idx;
} dummy_it_t;

static int dummy_read_link(sqfs_dir_iterator_t *it, char **out)
{
	(void)it; (void)out;
	TEST_ASSERT(0);
	return 0;
}

static void dummy_ignore_subdir(sqfs_dir_iterator_t *it)
{
	(void)it;
	TEST_ASSERT(0);
}

static int dummy_open_file_ro(sqfs_dir_iterator_t *it, sqfs_istream_t **out)
{
	(void)it; (void)out;
	TEST_ASSERT(0);
	return 0;
}

static int dummy_read_xattr(sqfs_dir_iterator_t *it, sqfs_xattr_t **out)
{
	(void)it; (void)out;
	TEST_ASSERT(0);
	return 0;
}

static void destroy(sqfs_object_t *obj)
{
	free(obj);
}

static int dummy_next(sqfs_dir_iterator_t *base, sqfs_dir_entry_t **out)
{
	dummy_it_t *it = (dummy_it_t *)base;
	char buffer[3];

	*out = NULL;
	if (it->idx >= 4)
		return 1;

	buffer[0] = 'a' + it->idx;
	buffer[1] = 'A' + it->idx;
	buffer[2] = '\0';

	if ((it->idx % 2) != 0 && it->level < 2) {
		*out = sqfs_dir_entry_create(buffer,
					     SQFS_INODE_MODE_DIR | 0755, 0);
		it->current_is_dir = true;
	} else {
		*out = sqfs_dir_entry_create(buffer,
					     SQFS_INODE_MODE_REG | 0644, 0);
		it->current_is_dir = false;
	}

	it->idx += 1;
	TEST_NOT_NULL((*out));
	return 0;
}

static int dummy_open_subdir(sqfs_dir_iterator_t *base,
			     sqfs_dir_iterator_t **out)
{
	dummy_it_t *it = (dummy_it_t *)base, *sub;

	TEST_ASSERT(it->current_is_dir);

	sub = calloc(1, sizeof(*sub));
	TEST_NOT_NULL(sub);
	sub->level = it->level + 1;

	sqfs_object_init(sub, destroy, NULL);
	((sqfs_dir_iterator_t *)sub)->read_link = dummy_read_link;
	((sqfs_dir_iterator_t *)sub)->ignore_subdir = dummy_ignore_subdir;
	((sqfs_dir_iterator_t *)sub)->open_file_ro = dummy_open_file_ro;
	((sqfs_dir_iterator_t *)sub)->read_xattr = dummy_read_xattr;
	((sqfs_dir_iterator_t *)sub)->next = dummy_next;
	((sqfs_dir_iterator_t *)sub)->open_subdir = dummy_open_subdir;

	*out = (sqfs_dir_iterator_t *)sub;
	return 0;
}

static sqfs_dir_iterator_t *mkdummyit(void)
{
	dummy_it_t *it = calloc(1, sizeof(*it));
	TEST_NOT_NULL(it);

	sqfs_object_init(it, destroy, NULL);
	((sqfs_dir_iterator_t *)it)->read_link = dummy_read_link;
	((sqfs_dir_iterator_t *)it)->ignore_subdir = dummy_ignore_subdir;
	((sqfs_dir_iterator_t *)it)->open_file_ro = dummy_open_file_ro;
	((sqfs_dir_iterator_t *)it)->read_xattr = dummy_read_xattr;
	((sqfs_dir_iterator_t *)it)->next = dummy_next;
	((sqfs_dir_iterator_t *)it)->open_subdir = dummy_open_subdir;
	return (sqfs_dir_iterator_t *)it;
}

static const struct {
	const char *name;
	bool isdir;
} expect[] = {
	{ "aA", false },
	{ "bB", true },
	{ "bB/aA", false },
	{ "bB/bB", true },
	{ "bB/bB/aA", false },
	{ "bB/bB/bB", false },
	{ "bB/bB/cC", false },
	{ "bB/bB/dD", false },
	{ "bB/cC", false },
	{ "bB/dD", true },
	{ "bB/dD/aA", false },
	{ "bB/dD/bB", false },
	{ "bB/dD/cC", false },
	{ "bB/dD/dD", false },
	{ "cC", false },
	{ "dD", true },
	{ "dD/aA", false },
	{ "dD/bB", true },
	{ "dD/bB/aA", false },
	{ "dD/bB/bB", false },
	{ "dD/bB/cC", false },
	{ "dD/bB/dD", false },
	{ "dD/cC", false },
	{ "dD/dD", true },
	{ "dD/dD/aA", false },
	{ "dD/dD/bB", false },
	{ "dD/dD/cC", false },
	{ "dD/dD/dD", false },
};

int main(int argc, char **argv)
{
	sqfs_dir_iterator_t *it, *rec;
	sqfs_dir_entry_t *ent;
	int ret;
	(void)argc; (void)argv;

	/* simple test of the dummy iterator */
	it = mkdummyit();

	ret = it->next(it, &ent);
	TEST_EQUAL_I(ret, 0);
	TEST_STR_EQUAL(ent->name, "aA");
	TEST_ASSERT(S_ISREG(ent->mode));
	free(ent);

	ret = it->next(it, &ent);
	TEST_EQUAL_I(ret, 0);
	TEST_STR_EQUAL(ent->name, "bB");
	TEST_ASSERT(S_ISDIR(ent->mode));
	free(ent);

	ret = it->next(it, &ent);
	TEST_EQUAL_I(ret, 0);
	TEST_STR_EQUAL(ent->name, "cC");
	TEST_ASSERT(S_ISREG(ent->mode));
	free(ent);

	ret = it->next(it, &ent);
	TEST_EQUAL_I(ret, 0);
	TEST_STR_EQUAL(ent->name, "dD");
	TEST_ASSERT(S_ISDIR(ent->mode));
	free(ent);

	ret = it->next(it, &ent);
	TEST_EQUAL_I(ret, 1);
	TEST_NULL(ent);

	sqfs_drop(it);

	/* construct recursive iterator */
	it = mkdummyit();

	ret = sqfs_dir_iterator_create_recursive(&rec, it);
	sqfs_drop(it);
	TEST_EQUAL_I(ret, 0);
	TEST_NOT_NULL(rec);
	it = rec;

	for (size_t i = 0; i < sizeof(expect) / sizeof(expect[0]); ++i) {
		ret = it->next(it, &ent);
		TEST_EQUAL_I(ret, 0);

		if (strcmp(ent->name, expect[i].name) != 0) {
			fprintf(stderr,
				"Entry %u should be `%s`, but is `%s`\n",
				(unsigned int)i, expect[i].name, ent->name);
			return EXIT_FAILURE;
		}

		if (expect[i].isdir && !S_ISDIR(ent->mode)) {
			fprintf(stderr,
				"Entry %u (`%s`) should be dir: "
				"mode is `%u`\n",
				(unsigned int)i, ent->name, ent->mode);
			return EXIT_FAILURE;
		} else if (!expect[i].isdir && !S_ISREG(ent->mode)) {
			fprintf(stderr,
				"Entry %u (`%s`) should be file: "
				"mode is `%u`\n",
				(unsigned int)i, ent->name, ent->mode);
			return EXIT_FAILURE;
		}

		free(ent);
	}

	ret = it->next(it, &ent);
	TEST_EQUAL_I(ret, 1);

	sqfs_drop(it);
	return EXIT_SUCCESS;
}
