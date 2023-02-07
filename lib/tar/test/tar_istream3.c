/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * tar_istream3.c
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
	tar_header_decoded_t hdr;
	istream_t *fp, *ti;
	uint64_t offset;
	sqfs_s32 i, ret;
	(void)argc; (void)argv;

	fp = istream_open_file(STRVALUE(TESTPATH) "/" STRVALUE(TESTFILE));
	TEST_NOT_NULL(fp);
	TEST_ASSERT(read_header(fp, &hdr) == 0);
	TEST_EQUAL_UI(hdr.mode, S_IFREG | 0644);
	TEST_EQUAL_UI(hdr.uid, 01750);
	TEST_EQUAL_UI(hdr.gid, 01750);
	TEST_EQUAL_UI(hdr.actual_size, 2097152);
	TEST_EQUAL_UI(hdr.record_size, 32768);
	TEST_STR_EQUAL(hdr.name, "input.bin");
	TEST_ASSERT(!hdr.unknown_record);

	ti = tar_record_istream_create(fp, &hdr);
	TEST_EQUAL_UI(((sqfs_object_t *)fp)->refcount, 2);
	TEST_EQUAL_UI(((sqfs_object_t *)ti)->refcount, 1);
	clear_header(&hdr);

	offset = 0;

	for (;;) {
		ret = istream_read(ti, buffer, sizeof(buffer));
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

	sqfs_drop(ti);
	TEST_EQUAL_UI(((sqfs_object_t *)fp)->refcount, 1);
	sqfs_drop(fp);
	return EXIT_SUCCESS;
}
