/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * hl_dir.c
 *
 * Copyright (C) 2023 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "util/test.h"
#include "sqfs/dir_entry.h"
#include "sqfs/inode.h"
#include "sqfs/io.h"
#include "compat.h"

typedef struct {
	sqfs_dir_iterator_t obj;
	size_t idx;
} dummy_it_t;

static const struct {
	const char *name;
	int dev;
	int inum;
} entries[] = {
	{ "foo", 1, 1 },
	{ "bar", 1, 2 },
	{ "baz", 1, 3 },
	{ "blub", 1, 2 },
	{ "a", 2, 2 },
	{ "b", 2, 1 },
	{ "c", 2, 2 },
};

static int dummy_read_link(sqfs_dir_iterator_t *it, char **out)
{
	(void)it; (void)out;
	*out = NULL;
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

static int dummy_open_subdir(sqfs_dir_iterator_t *base,
			     sqfs_dir_iterator_t **out)
{
	(void)base; (void)out;
	TEST_ASSERT(0);
	return 0;
}

static int dummy_next(sqfs_dir_iterator_t *base, sqfs_dir_entry_t **out)
{
	dummy_it_t *it = (dummy_it_t *)base;
	const char *name;
	int inum, dev;

	if (it->idx >= (sizeof(entries) / sizeof(entries[0])))
		return 1;

	name = entries[it->idx].name;
	inum = entries[it->idx].inum;
	dev = entries[it->idx].dev;
	it->idx += 1;

	*out = sqfs_dir_entry_create(name, SQFS_INODE_MODE_REG | 0644, 0);
	TEST_NOT_NULL(*out);
	(*out)->inode = inum;
	(*out)->dev = dev;
	return 0;
}

static void dummy_destroy(sqfs_object_t *obj)
{
	free(obj);
}

static sqfs_dir_iterator_t *mkdummyit(void)
{
	dummy_it_t *it = calloc(1, sizeof(*it));
	TEST_NOT_NULL(it);

	sqfs_object_init(it, dummy_destroy, NULL);
	((sqfs_dir_iterator_t *)it)->read_link = dummy_read_link;
	((sqfs_dir_iterator_t *)it)->ignore_subdir = dummy_ignore_subdir;
	((sqfs_dir_iterator_t *)it)->open_file_ro = dummy_open_file_ro;
	((sqfs_dir_iterator_t *)it)->read_xattr = dummy_read_xattr;
	((sqfs_dir_iterator_t *)it)->next = dummy_next;
	((sqfs_dir_iterator_t *)it)->open_subdir = dummy_open_subdir;
	return (sqfs_dir_iterator_t *)it;
}

int main(int argc, char **argv)
{
	sqfs_dir_iterator_t *base, *it;
	sqfs_dir_entry_t *ent;
	char *target;
	int ret;
	(void)argc; (void)argv;

	base = mkdummyit();
	TEST_NOT_NULL(base);

	ret = sqfs_hard_link_filter_create(&it, base);
	TEST_EQUAL_I(ret, 0);
	TEST_NOT_NULL(it);
	TEST_EQUAL_UI(((sqfs_object_t *)base)->refcount, 2);

	ret = it->next(it, &ent);
	TEST_EQUAL_I(ret, 0);
	TEST_NOT_NULL(&ent);
	TEST_STR_EQUAL(ent->name, "foo");
	TEST_ASSERT(S_ISREG(ent->mode));
	TEST_EQUAL_UI(ent->flags, 0);
	ret = it->read_link(it, &target);
	TEST_EQUAL_I(ret, 0);
	TEST_NULL(target);
	free(ent);

	ret = it->next(it, &ent);
	TEST_EQUAL_I(ret, 0);
	TEST_NOT_NULL(&ent);
	TEST_STR_EQUAL(ent->name, "bar");
	TEST_ASSERT(S_ISREG(ent->mode));
	TEST_EQUAL_UI(ent->flags, 0);
	ret = it->read_link(it, &target);
	TEST_EQUAL_I(ret, 0);
	TEST_NULL(target);
	free(ent);

	ret = it->next(it, &ent);
	TEST_EQUAL_I(ret, 0);
	TEST_NOT_NULL(&ent);
	TEST_STR_EQUAL(ent->name, "baz");
	TEST_ASSERT(S_ISREG(ent->mode));
	TEST_EQUAL_UI(ent->flags, 0);
	ret = it->read_link(it, &target);
	TEST_EQUAL_I(ret, 0);
	TEST_NULL(target);
	free(ent);

	ret = it->next(it, &ent);
	TEST_EQUAL_I(ret, 0);
	TEST_NOT_NULL(&ent);
	TEST_STR_EQUAL(ent->name, "blub");
	TEST_EQUAL_UI(ent->flags, SQFS_DIR_ENTRY_FLAG_HARD_LINK);
	TEST_ASSERT(S_ISLNK(ent->mode));
	ret = it->read_link(it, &target);
	TEST_EQUAL_I(ret, 0);
	TEST_NOT_NULL(target);
	TEST_STR_EQUAL(target, "bar");
	free(target);
	free(ent);

	ret = it->next(it, &ent);
	TEST_EQUAL_I(ret, 0);
	TEST_NOT_NULL(&ent);
	TEST_STR_EQUAL(ent->name, "a");
	TEST_ASSERT(S_ISREG(ent->mode));
	TEST_EQUAL_UI(ent->flags, 0);
	ret = it->read_link(it, &target);
	TEST_EQUAL_I(ret, 0);
	TEST_NULL(target);
	free(ent);

	ret = it->next(it, &ent);
	TEST_EQUAL_I(ret, 0);
	TEST_NOT_NULL(&ent);
	TEST_STR_EQUAL(ent->name, "b");
	TEST_ASSERT(S_ISREG(ent->mode));
	TEST_EQUAL_UI(ent->flags, 0);
	ret = it->read_link(it, &target);
	TEST_EQUAL_I(ret, 0);
	TEST_NULL(target);
	free(ent);

	ret = it->next(it, &ent);
	TEST_EQUAL_I(ret, 0);
	TEST_NOT_NULL(&ent);
	TEST_STR_EQUAL(ent->name, "c");
	TEST_EQUAL_UI(ent->flags, SQFS_DIR_ENTRY_FLAG_HARD_LINK);
	TEST_ASSERT(S_ISLNK(ent->mode));
	ret = it->read_link(it, &target);
	TEST_EQUAL_I(ret, 0);
	TEST_NOT_NULL(target);
	TEST_STR_EQUAL(target, "a");
	free(target);
	free(ent);

	ret = it->next(it, &ent);
	TEST_EQUAL_I(ret, 1);
	TEST_NULL(ent);
	it = sqfs_drop(it);

	TEST_EQUAL_UI(((sqfs_object_t *)base)->refcount, 1);
	base = sqfs_drop(base);
	return EXIT_SUCCESS;
}
