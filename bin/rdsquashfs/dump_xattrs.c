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
	sqfs_xattr_value_t *value;
	sqfs_xattr_entry_t *key;
	sqfs_xattr_id_t desc;
	sqfs_u32 index;
	size_t i;

	if (xattr == NULL)
		return 0;

	sqfs_inode_get_xattr_index(inode, &index);

	if (index == 0xFFFFFFFF)
		return 0;

	if (sqfs_xattr_reader_get_desc(xattr, index, &desc)) {
		fputs("Error resolving xattr index\n", stderr);
		return -1;
	}

	if (sqfs_xattr_reader_seek_kv(xattr, &desc)) {
		fputs("Error locating xattr key-value pairs\n", stderr);
		return -1;
	}

	for (i = 0; i < desc.count; ++i) {
		if (sqfs_xattr_reader_read_key(xattr, &key)) {
			fputs("Error reading xattr key\n", stderr);
			return -1;
		}

		if (sqfs_xattr_reader_read_value(xattr, key, &value)) {
			fputs("Error reading xattr value\n", stderr);
			sqfs_free(key);
			return -1;
		}

		if (is_printable(key->key, key->size)) {
			printf("%s=", key->key);
		} else {
			print_hex(key->key, key->size);
		}

		if (is_printable(value->value, value->size)) {
			printf("%s\n", value->value);
		} else {
			print_hex(value->value, value->size);
			printf("\n");
		}

		sqfs_free(key);
		sqfs_free(value);
	}

	return 0;
}
