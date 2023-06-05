/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * xattr.c
 *
 * Copyright (C) 2023 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "compat.h"
#include "util/test.h"

#include "sqfs/xattr.h"
#include "sqfs/error.h"

int main(int argc, char **argv)
{
	sqfs_xattr_t *ent, *ent2, *list;
	const char *str;
	int id, ret;
	(void)argc; (void)argv;

	/* prefix API */
	id = sqfs_get_xattr_prefix_id("user.mime_type");
	TEST_EQUAL_I(id, SQFS_XATTR_USER);

	str = sqfs_get_xattr_prefix(id);
	TEST_STR_EQUAL(str, "user.");

	id = sqfs_get_xattr_prefix_id("security.selinux");
	TEST_EQUAL_I(id, SQFS_XATTR_SECURITY);

	str = sqfs_get_xattr_prefix(id);
	TEST_STR_EQUAL(str, "security.");

	id = sqfs_get_xattr_prefix_id("trusted.bla");
	TEST_EQUAL_I(id, SQFS_XATTR_TRUSTED);

	str = sqfs_get_xattr_prefix(id);
	TEST_STR_EQUAL(str, "trusted.");

	id = sqfs_get_xattr_prefix_id("system.acl");
	TEST_EQUAL_I(id, SQFS_ERROR_UNSUPPORTED);

	str = sqfs_get_xattr_prefix(id);
	TEST_NULL(str);

	/* combined entry API */
	ent = sqfs_xattr_create("foo.bar", (const sqfs_u8 *)"Hello, World!", 5);
	TEST_NOT_NULL(ent);
	TEST_EQUAL_UI(ent->value_len, 5);
	TEST_STR_EQUAL(ent->key, "foo.bar");
	TEST_STR_EQUAL((const char *)ent->value, "Hello");
	sqfs_xattr_list_free(ent);

	/* with prefix */
	ret = sqfs_xattr_create_prefixed(&ent, SQFS_XATTR_SECURITY, "selinux",
					 (const sqfs_u8 *)"Hello, World!", 5);
	TEST_EQUAL_I(ret, 0);
	TEST_NOT_NULL(ent);
	TEST_EQUAL_UI(ent->value_len, 5);
	TEST_STR_EQUAL(ent->key, "security.selinux");
	TEST_STR_EQUAL((const char *)ent->value, "Hello");
	sqfs_xattr_list_free(ent);

	ret = sqfs_xattr_create_prefixed(&ent, 42, "selinux",
					 (const sqfs_u8 *)"Hello, World!", 5);
	TEST_EQUAL_I(ret, SQFS_ERROR_UNSUPPORTED);
	TEST_NULL(ent);

	/* list copy */
	ent = sqfs_xattr_create("foo.bar", (const sqfs_u8 *)"Hello, World!", 5);
	TEST_NOT_NULL(ent);

	ent2 = sqfs_xattr_create("bla.blu", (const sqfs_u8 *)"test", 4);
	TEST_NOT_NULL(ent2);

	list = sqfs_xattr_list_copy(NULL);
	TEST_NULL(list);

	list = sqfs_xattr_list_copy(ent);
	TEST_NOT_NULL(list);
	TEST_ASSERT(list != ent);
	TEST_STR_EQUAL(list->key, ent->key);
	TEST_STR_EQUAL((const char *)list->value, (const char *)ent->value);
	TEST_EQUAL_UI(list->value_len, ent->value_len);
	sqfs_xattr_list_free(list);

	ent->next = ent2;
	ent2->next = NULL;

	list = sqfs_xattr_list_copy(ent);
	TEST_NOT_NULL(list);
	TEST_ASSERT(list != ent && list != ent2);
	TEST_STR_EQUAL(list->key, ent->key);
	TEST_STR_EQUAL((const char *)list->value, (const char *)ent->value);
	TEST_EQUAL_UI(list->value_len, ent->value_len);

	TEST_NOT_NULL(list->next);
	TEST_ASSERT(list->next != ent && list->next != ent2);
	TEST_STR_EQUAL(list->next->key, ent2->key);
	TEST_STR_EQUAL((const char *)list->next->value,
		       (const char *)ent2->value);
	TEST_EQUAL_UI(list->next->value_len, ent2->value_len);
	sqfs_xattr_list_free(list);

	ent->next = NULL;
	ent2->next = NULL;
	sqfs_xattr_list_free(ent);
	sqfs_xattr_list_free(ent2);

	return EXIT_SUCCESS;
}
