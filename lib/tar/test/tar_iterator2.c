/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * tar_iterator2.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "io/file.h"
#include "tar/tar.h"
#include "util/test.h"

static const struct {
	uint64_t offset;
	size_t size;
	int fill;
} regions[] = {
	{       0, 4096, 'A' },
	{  262144, 4096, 'B' },
	{  524288, 4096, 'C' },
	{  786432, 4096, 'D' },
	{ 1048576, 4096, 'E' },
	{ 1310720, 4096, 'F' },
	{ 1572864, 4096, 'G' },
	{ 1835008, 4096, 'H' },
};

static int byte_from_offset(uint64_t offset)
{
	sqfs_u64 diff;
	size_t i;

	for (i = 0; i < sizeof(regions) / sizeof(regions[0]); ++i) {
		if (offset >= regions[i].offset) {
			diff = (offset - regions[i].offset);

			if (diff < regions[i].size)
				return regions[i].fill;
		}
	}

	return '\0';
}

int main(int argc, char **argv)
{
	unsigned char buffer[941];
	sqfs_istream_t *fp, *ti;
	dir_iterator_t *it;
	dir_entry_t *ent;
	uint64_t offset;
	sqfs_s32 i, ret;
	(void)argc; (void)argv;

	fp = istream_open_file(STRVALUE(TESTPATH) "/" STRVALUE(TESTFILE));
	TEST_NOT_NULL(fp);
	it = tar_open_stream(fp);
	TEST_NOT_NULL(it);
	sqfs_drop(fp);

	ret = it->next(it, &ent);
	TEST_EQUAL_I(ret, 0);
	TEST_NOT_NULL(ent);

	TEST_EQUAL_UI(ent->mode, S_IFREG | 0644);
	TEST_EQUAL_UI(ent->uid, 01750);
	TEST_EQUAL_UI(ent->gid, 01750);
	TEST_EQUAL_UI(ent->size, 2097152);
	//TEST_EQUAL_UI(ent->on_disk_size, 32768);
	TEST_STR_EQUAL(ent->name, "input.bin");
	free(ent);

	ret = it->open_file_ro(it, &ti);
	TEST_EQUAL_I(ret, 0);
	TEST_NOT_NULL(ti);

	offset = 0;

	for (;;) {
		ret = sqfs_istream_read(ti, buffer, sizeof(buffer));
		TEST_ASSERT(ret >= 0);

		if (ret == 0)
			break;

		for (i = 0; i < ret; ++i) {
			int ref_byte = byte_from_offset(offset + i);

			if (buffer[i] != ref_byte) {
				fprintf(stderr, "Byte at offset %llu should "
					"be 0x%02X, but is 0x%02X\n",
					(unsigned long long)(offset + i),
					(unsigned int)ref_byte, buffer[i]);
				return EXIT_FAILURE;
			}
		}

		offset += ret;
		TEST_ASSERT(offset <= 2097152);
	}

	TEST_EQUAL_UI(offset, 2097152);

	sqfs_drop(it);
	sqfs_drop(ti);
	return EXIT_SUCCESS;
}
