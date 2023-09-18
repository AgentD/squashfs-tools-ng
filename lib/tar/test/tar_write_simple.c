/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * tar_write_simple.c
 *
 * Copyright (C) 2023 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "tar/tar.h"
#include "sqfs/io.h"
#include "util/test.h"
#include "sqfs/xattr.h"
#include "compat.h"

static void hex_dump(const sqfs_u8 *data, size_t size)
{
	for (size_t i = 0; i < size; ++i) {
		int hi = (data[i] >> 4) & 0x0F;
		int lo =  data[i]       & 0x0F;

		hi = (hi >= 0x0a) ? ('a' + (hi - 0x0a)) : ('0' + hi);
		lo = (lo >= 0x0a) ? ('a' + (lo - 0x0a)) : ('0' + lo);

		fprintf(stderr, "%c%c", hi, lo);

		if ((i % 16) == 15) {
			fputs(" | ", stderr);

			for (size_t j = i - 15; j <= i; ++j) {
				if (data[j] >= 0x20 && data[j] <= 0x7f) {
					fputc(data[j], stderr);
				} else {
					fputc('.', stderr);
				}
			}

			fputc('\n', stderr);
		} else {
			fputc(' ', stderr);
		}
	}
}

/*****************************************************************************/

static int buffer_append(sqfs_ostream_t *strm, const void *data, size_t size);
static const char *buffer_get_filename(sqfs_ostream_t *strm);

static sqfs_ostream_t mem_stream = {
	{ 1, NULL, NULL },
	buffer_append,
	NULL,
	buffer_get_filename,
};

static sqfs_u8 wr_buffer[1024 * 10];
static size_t wr_offset = 0;

static sqfs_u8 rd_buffer[1024 * 10];

static int buffer_append(sqfs_ostream_t *strm, const void *data, size_t size)
{
	TEST_ASSERT(strm == &mem_stream);
	TEST_ASSERT(wr_offset < sizeof(wr_buffer));
	TEST_ASSERT(size > 0);
	TEST_ASSERT((sizeof(wr_buffer) - wr_offset) >= size);

	if (data == NULL) {
		memset(wr_buffer + wr_offset, 0, size);
	} else {
		memcpy(wr_buffer + wr_offset, data, size);
	}

	wr_offset += size;
	return 0;
}

static const char *buffer_get_filename(sqfs_ostream_t *strm)
{
	TEST_ASSERT(strm == &mem_stream);
	return "dummy";
}

/*****************************************************************************/

#define TIME_STAMP (1057296600)

static sqfs_xattr_t *mkxattr_chain(void)
{
	static const uint8_t value[] = {
		0x00, 0x00, 0x00, 0x02,
		0x00, 0x30, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
	};
	sqfs_xattr_t *list;

	list = sqfs_xattr_create("user.mime_type",
				 (const sqfs_u8 *)"blob/magic", 10);
	TEST_NOT_NULL(list);
	list->next = sqfs_xattr_create("security.capability",
				       value, sizeof(value));
	TEST_NOT_NULL(list->next);
	return list;
}

int main(int argc, char **argv)
{
	sqfs_dir_entry_t *ent;
	sqfs_xattr_t *xattr;
	sqfs_istream_t *fp;
	int ret;
	(void)argc; (void)argv;

	/* genereate some archive contents */
	ent = sqfs_dir_entry_create("dev/", S_IFDIR | 0755, 0);
	TEST_NOT_NULL(ent);
	ent->mtime = TIME_STAMP;
	ret = write_tar_header(&mem_stream, ent, NULL, NULL, 0);
	sqfs_free(ent);
	TEST_EQUAL_I(ret, 0);

	/* device files */
	ent = sqfs_dir_entry_create("dev/tty0", S_IFCHR | 0620, 0);
	TEST_NOT_NULL(ent);
	ent->mtime = TIME_STAMP;
	ent->gid = 5;
	ent->rdev = makedev(4, 0);
	ret = write_tar_header(&mem_stream, ent, NULL, NULL, 1);
	sqfs_free(ent);
	TEST_EQUAL_I(ret, 0);

	ent = sqfs_dir_entry_create("dev/tty1", S_IFCHR | 0620, 0);
	TEST_NOT_NULL(ent);
	ent->mtime = TIME_STAMP;
	ent->gid = 5;
	ent->rdev = makedev(4, 1);
	ret = write_tar_header(&mem_stream, ent, NULL, NULL, 2);
	sqfs_free(ent);
	TEST_EQUAL_I(ret, 0);

	ent = sqfs_dir_entry_create("dev/tty2", S_IFCHR | 0620, 0);
	TEST_NOT_NULL(ent);
	ent->mtime = TIME_STAMP;
	ent->gid = 5;
	ent->rdev = makedev(4, 2);
	ret = write_tar_header(&mem_stream, ent, NULL, NULL, 3);
	sqfs_free(ent);
	TEST_EQUAL_I(ret, 0);

	ent = sqfs_dir_entry_create("usr/", S_IFDIR | 0755, 0);
	TEST_NOT_NULL(ent);
	ent->mtime = TIME_STAMP;
	ret = write_tar_header(&mem_stream, ent, NULL, NULL, 4);
	sqfs_free(ent);
	TEST_EQUAL_I(ret, 0);

	ent = sqfs_dir_entry_create("usr/bin/", S_IFDIR | 0755, 0);
	TEST_NOT_NULL(ent);
	ent->mtime = TIME_STAMP;
	ret = write_tar_header(&mem_stream, ent, NULL, NULL, 5);
	sqfs_free(ent);
	TEST_EQUAL_I(ret, 0);

	/* sym link */
	ent = sqfs_dir_entry_create("bin", S_IFLNK | 0777, 0);
	TEST_NOT_NULL(ent);
	ent->mtime = TIME_STAMP;
	ent->size = 7;
	ret = write_tar_header(&mem_stream, ent, "usr/bin", NULL, 6);
	sqfs_free(ent);
	TEST_EQUAL_I(ret, 0);

	ent = sqfs_dir_entry_create("home/", S_IFDIR | 0755, 0);
	TEST_NOT_NULL(ent);
	ent->mtime = TIME_STAMP;
	ret = write_tar_header(&mem_stream, ent, NULL, NULL, 7);
	sqfs_free(ent);
	TEST_EQUAL_I(ret, 0);

	ent = sqfs_dir_entry_create("home/goliath/", S_IFDIR | 0750, 0);
	TEST_NOT_NULL(ent);
	ent->uid = 1000;
	ent->gid = 1000;
	ent->mtime = TIME_STAMP;
	ret = write_tar_header(&mem_stream, ent, NULL, NULL, 8);
	sqfs_free(ent);
	TEST_EQUAL_I(ret, 0);

	/* regular file with actual content */
	ent = sqfs_dir_entry_create("home/goliath/hello.txt", S_IFREG | 0644, 0);
	TEST_NOT_NULL(ent);
	ent->uid = 1000;
	ent->gid = 1000;
	ent->mtime = TIME_STAMP;
	ent->size = 14;
	ret = write_tar_header(&mem_stream, ent, NULL, NULL, 9);
	TEST_EQUAL_I(ret, 0);

	ret = mem_stream.append(&mem_stream, "Hello, World!\n", 14);
	TEST_EQUAL_I(ret, 0);
	ret = padd_file(&mem_stream, 14);
	TEST_EQUAL_I(ret, 0);

	/* hard link */
	strcpy(ent->name, "home/goliath/world.txt");
	ent->size = 22;
	ent->flags = SQFS_DIR_ENTRY_FLAG_HARD_LINK;
	ret = write_tar_header(&mem_stream, ent, "home/goliath/hello.txt",
			       NULL, 10);
	TEST_EQUAL_I(ret, 0);

	/* something with xattrs */
	strcpy(ent->name, "home/goliath/test.exe");
	ent->flags = 0;
	ent->mode = S_IFREG | 0750;
	ent->size = 4;

	xattr = mkxattr_chain();

	ret = write_tar_header(&mem_stream, ent, NULL, xattr, 11);
	TEST_EQUAL_I(ret, 0);
	sqfs_free(ent);
	sqfs_xattr_list_free(xattr);

	ret = mem_stream.append(&mem_stream, ":-)\n", 4);
	TEST_EQUAL_I(ret, 0);
	ret = padd_file(&mem_stream, 4);
	TEST_EQUAL_I(ret, 0);

	/* now try something with a long name */
	ent = sqfs_dir_entry_create("mnt/windows_drive/C/Documents and Settings/"
				    "Joe Random User/My Documents/My Evil Plans/"
				    "file format nonsense/really long name.doc",
				    S_IFREG | 0755, 0);
	TEST_NOT_NULL(ent);
	ent->mtime = TIME_STAMP;
	ent->size = 42;

	ret = write_tar_header(&mem_stream, ent, NULL, NULL, 12);
	sqfs_free(ent);
	TEST_EQUAL_I(ret, 0);

	ret = mem_stream.append(&mem_stream,
				"Annoy people with really long file paths!\n",
				42);
	TEST_EQUAL_I(ret, 0);
	ret = padd_file(&mem_stream, 42);
	TEST_EQUAL_I(ret, 0);

	/* compare with reference */
	TEST_EQUAL_UI(sizeof(rd_buffer), sizeof(wr_buffer));

	ret = sqfs_istream_open_file(&fp,
				     STRVALUE(TESTPATH) "/" STRVALUE(TESTFILE),
				     0);
	TEST_EQUAL_I(ret, 0);
	TEST_NOT_NULL(fp);

	ret = sqfs_istream_read(fp, rd_buffer, sizeof(rd_buffer));
	TEST_ASSERT(ret > 0);
	TEST_EQUAL_UI(ret, sizeof(rd_buffer));

	ret = sqfs_istream_read(fp, rd_buffer, sizeof(rd_buffer));
	TEST_EQUAL_I(ret, 0);

	sqfs_drop(fp);

	if (wr_offset != sizeof(wr_buffer)) {
		fprintf(stderr, "Result data size should be: %u, "
			"but actually is: %u\n", (unsigned int)wr_offset,
			(unsigned int)sizeof(wr_buffer));
		ret = -1;
	}

	for (size_t i = 0; i < wr_offset; i += 512) {
		size_t diff = (wr_offset - i) > 512 ? 512 : (wr_offset - i);

		if (memcmp(wr_buffer + i, rd_buffer + i, diff) != 0) {
			fprintf(stderr, "Difference at offset %u:\n",
				(unsigned int)i);

			fputs("Reference:\n", stderr);
			hex_dump(rd_buffer + i, diff);

			fputs("Result:\n", stderr);
			hex_dump(wr_buffer + i, diff);
			ret = -1;
			break;
		}
	}

	return ret == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
