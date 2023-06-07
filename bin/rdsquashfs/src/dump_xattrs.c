/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * dump_xattrs.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "rdsquashfs.h"

static void print_hex(const sqfs_u8 *value, size_t len)
{
	printf("0x");

	while (len--)
		printf("%02X", *(value++));
}

static bool is_printable(const sqfs_u8 *value, size_t len)
{
	size_t utf8_cont = 0;
	sqfs_u8 x;

	while (len--) {
		x = *(value++);

		if (utf8_cont > 0) {
			if ((x & 0xC0) != 0x80)
				return false;

			--utf8_cont;
		} else {
			if (x < 0x80) {
				if (x < 0x20) {
					if (x >= 0x07 && x <= 0x0D)
						continue;
					if (x == 0x00)
						continue;
					return false;
				}

				if (x == 0x7F)
					return false;
			}

			if ((x & 0xE0) == 0xC0) {
				utf8_cont = 1;
			} else if ((x & 0xF0) == 0xE0) {
				utf8_cont = 2;
			} else if ((x & 0xF8) == 0xF0) {
				utf8_cont = 3;
			} else if ((x & 0xFC) == 0xF8) {
				utf8_cont = 4;
			} else if ((x & 0xFE) == 0xFC) {
				utf8_cont = 5;
			}

			if (utf8_cont > 0 && len < utf8_cont)
				return false;
		}
	}

	return true;
}

int dump_xattrs(sqfs_xattr_reader_t *xattr, const sqfs_inode_generic_t *inode)
{
	sqfs_xattr_t *list;
	sqfs_u32 index;

	if (xattr == NULL)
		return 0;

	sqfs_inode_get_xattr_index(inode, &index);

	if (sqfs_xattr_reader_read_all(xattr, index, &list)) {
		fprintf(stderr, "Error loading xattr entries list #%08X\n",
			index);
		return -1;
	}

	for (const sqfs_xattr_t *ent = list; ent != NULL; ent = ent->next) {
		size_t key_len = strlen(ent->key);

		if (is_printable((const sqfs_u8 *)ent->key, key_len)) {
			printf("%s=", ent->key);
		} else {
			print_hex((const sqfs_u8 *)ent->key, key_len);
		}

		if (is_printable(ent->value, ent->value_len)) {
			printf("%s\n", ent->value);
		} else {
			print_hex(ent->value, ent->value_len);
			printf("\n");
		}
	}

	sqfs_xattr_list_free(list);
	return 0;
}
