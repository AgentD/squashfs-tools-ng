/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * filemap_xattr.c
 *
 * Copyright (C) 2022 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "util/test.h"
#include "mkfs.h"

static const char *dev_selinux = "system_u:object_r:device_t:s0";
static const char *zero_selinux = "system_u:object_r:zero_device_t:s0";
static const char *rfkill_selinux = "system_u:object_r:wireless_device_t:s0";

static const sqfs_u8 rfkill_acl[] = {
	0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x06, 0x00,
	0xff, 0xff, 0xff, 0xff, 0x02, 0x00, 0x06, 0x00,
	0xe8, 0x03, 0x00, 0x00, 0x04, 0x00, 0x06, 0x00,
	0xff, 0xff, 0xff, 0xff, 0x10, 0x00, 0x06, 0x00,
	0xff, 0xff, 0xff, 0xff, 0x20, 0x00, 0x04, 0x00,
	0xff, 0xff, 0xff, 0xff
};

int main(int argc, char **argv)
{
	struct XattrMapPattern *pat;
	struct XattrMapEntry *ent;
	struct XattrMap *map;
	int ret;
	(void)argc; (void)argv;

	map = xattr_open_map_file(TEST_PATH);
	TEST_NOT_NULL(map);

	/* the third pattern */
	pat = map->patterns;
	TEST_NOT_NULL(pat);
	TEST_STR_EQUAL(pat->path, "dev/rfkill");

	ent = pat->entries;
	TEST_NOT_NULL(ent);
	TEST_STR_EQUAL(ent->key, "system.posix_acl_access");

	TEST_EQUAL_UI(ent->value_len, sizeof(rfkill_acl));
	ret = memcmp(ent->value, rfkill_acl, ent->value_len);
	TEST_EQUAL_I(ret, 0);

	ent = ent->next;
	TEST_NOT_NULL(ent);
	TEST_STR_EQUAL(ent->key, "security.selinux");

	TEST_EQUAL_UI(ent->value_len, strlen(rfkill_selinux));
	ret = memcmp(ent->value, rfkill_selinux, ent->value_len);
	TEST_EQUAL_I(ret, 0);

	ent = ent->next;
	TEST_NULL(ent);

	/* the second pattern */
	pat = pat->next;
	TEST_NOT_NULL(pat);
	TEST_STR_EQUAL(pat->path, "dev/zero");

	ent = pat->entries;
	TEST_NOT_NULL(ent);
	TEST_STR_EQUAL(ent->key, "security.selinux");

	TEST_EQUAL_UI(ent->value_len, strlen(zero_selinux));
	ret = memcmp(ent->value, zero_selinux, ent->value_len);
	TEST_EQUAL_I(ret, 0);

	ent = ent->next;
	TEST_NULL(ent);

	/* the first pattern */
	pat = pat->next;
	TEST_NOT_NULL(pat);
	TEST_STR_EQUAL(pat->path, "dev");

	ent = pat->entries;
	TEST_NOT_NULL(ent);
	TEST_STR_EQUAL(ent->key, "security.selinux");

	TEST_EQUAL_UI(ent->value_len, strlen(dev_selinux));
	ret = memcmp(ent->value, dev_selinux, ent->value_len);
	TEST_EQUAL_I(ret, 0);

	ent = ent->next;
	TEST_NULL(ent);

	/* no more patterns */
	pat = pat->next;
	TEST_NULL(pat);

	xattr_close_map_file(map);
	return EXIT_SUCCESS;
}
