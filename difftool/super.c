/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * super.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "sqfsdiff.h"

static const struct {
	sqfs_u16 mask;
	const char *name;
} sqfs_flags[] = {
	{ SQFS_FLAG_UNCOMPRESSED_INODES, "uncompressed inodes" },
	{ SQFS_FLAG_UNCOMPRESSED_DATA, "uncompressed data" },
	{ SQFS_FLAG_UNCOMPRESSED_FRAGMENTS, "uncompressed fragments" },
	{ SQFS_FLAG_NO_FRAGMENTS, "no fragments" },
	{ SQFS_FLAG_ALWAYS_FRAGMENTS, "always fragments" },
	{ SQFS_FLAG_DUPLICATES, "duplicates" },
	{ SQFS_FLAG_EXPORTABLE, "exportable" },
	{ SQFS_FLAG_UNCOMPRESSED_XATTRS, "uncompressed xattrs" },
	{ SQFS_FLAG_NO_XATTRS, "no xattrs" },
	{ SQFS_FLAG_COMPRESSOR_OPTIONS, "compressor options" },
	{ SQFS_FLAG_UNCOMPRESSED_IDS, "uncompressed ids" },
};

static void print_value_difference(const char *name, sqfs_u64 a, sqfs_u64 b)
{
	sqfs_u64 diff;
	char c;

	if (a != b) {
		if (a < b) {
			c = '+';
			diff = b - a;
		} else {
			c = '-';
			diff = a - b;
		}
		fprintf(stdout, "%s: %c%llu\n", name, c,
			(unsigned long long)diff);
	}
}

static void print_offset_diff(const char *name, sqfs_u64 a, sqfs_u64 b)
{
	if (a != b)
		fprintf(stdout, "Location of %s differs\n", name);
}

static void print_flag_diff(sqfs_u16 a, sqfs_u16 b)
{
	sqfs_u16 diff = a ^ b, mask;
	size_t i;
	char c;

	if (diff == 0)
		return;

	fputs("flags:\n", stdout);

	for (i = 0; i < sizeof(sqfs_flags) / sizeof(sqfs_flags[0]); ++i) {
		if (diff & sqfs_flags[i].mask) {
			c = a & sqfs_flags[i].mask ? '<' : '>';

			fprintf(stdout, "\t%c%s\n", c, sqfs_flags[i].name);
		}

		a &= ~sqfs_flags[i].mask;
		b &= ~sqfs_flags[i].mask;
		diff &= ~sqfs_flags[i].mask;
	}

	for (i = 0, mask = 0x01; i < 16; ++i, mask <<= 1) {
		if (diff & mask) {
			fprintf(stdout, "\t%c additional unknown\n",
				a & mask ? '<' : '>');
		}
	}
}

int compare_super_blocks(const sqfs_super_t *a, const sqfs_super_t *b)
{
	if (memcmp(a, b, sizeof(*a)) == 0)
		return 0;

	fputs("======== super blocks are different ========\n", stdout);

	/* TODO: if a new magic number or squashfs version is introduced,
	   compare them. */

	print_value_difference("inode count", a->inode_count, b->inode_count);
	print_value_difference("modification time", a->modification_time,
			       b->modification_time);
	print_value_difference("block size", a->block_size, b->block_size);
	print_value_difference("block log", a->block_log, b->block_log);
	print_value_difference("fragment table entries",
			       a->fragment_entry_count,
			       b->fragment_entry_count);
	print_value_difference("ID table entries", a->id_count, b->id_count);

	if (a->compression_id != b->compression_id) {
		fprintf(stdout, "compressor: %s vs %s\n",
			sqfs_compressor_name_from_id(a->compression_id),
			sqfs_compressor_name_from_id(b->compression_id));
	}

	print_flag_diff(a->flags, b->flags);

	print_value_difference("total bytes used", a->bytes_used,
			       b->bytes_used);

	print_offset_diff("root inode", a->root_inode_ref, b->root_inode_ref);
	print_offset_diff("ID table", a->id_table_start, b->id_table_start);
	print_offset_diff("xattr ID table", a->xattr_id_table_start,
			  b->xattr_id_table_start);
	print_offset_diff("inode table", a->inode_table_start,
			  b->inode_table_start);
	print_offset_diff("directory table", a->directory_table_start,
			  b->directory_table_start);
	print_offset_diff("fragment table", a->fragment_table_start,
			  b->fragment_table_start);
	print_offset_diff("export table", a->export_table_start,
			  b->export_table_start);
	return 1;
}
