/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * tar_write_simple.c
 *
 * Copyright (C) 2023 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "tar/tar.h"
#include "io/ostream.h"
#include "io/file.h"
#include "util/test.h"
#include "compat.h"

/*****************************************************************************/

static int buffer_append(ostream_t *strm, const void *data, size_t size);
static const char *buffer_get_filename(ostream_t *strm);

static ostream_t mem_stream = {
	{ 1, NULL, NULL },
	buffer_append,
	NULL,
	NULL,
	buffer_get_filename,
};

static sqfs_u8 wr_buffer[1024 * 10];
static size_t wr_offset = 0;

static sqfs_u8 rd_buffer[1024 * 10];

static int buffer_append(ostream_t *strm, const void *data, size_t size)
{
	TEST_ASSERT(strm == &mem_stream);
	TEST_NOT_NULL(data);
	TEST_ASSERT(wr_offset < sizeof(wr_buffer));
	TEST_ASSERT(size > 0);
	TEST_ASSERT((sizeof(wr_buffer) - wr_offset) >= size);

	memcpy(wr_buffer + wr_offset, data, size);
	wr_offset += size;
	return 0;
}

static const char *buffer_get_filename(ostream_t *strm)
{
	TEST_ASSERT(strm == &mem_stream);
	return "dummy";
}

/*****************************************************************************/

#define TIME_STAMP (1057296600)

static dir_entry_xattr_t *mkxattr_chain(void)
{
	static const uint8_t value[] = {
		0x00, 0x00, 0x00, 0x02,
		0x00, 0x30, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
	};
	dir_entry_xattr_t *list;

	list = dir_entry_xattr_create("user.mime_type",
				      (const sqfs_u8 *)"blob/magic", 10);
	TEST_NOT_NULL(list);
	list->next = dir_entry_xattr_create("security.capability",
					    value, sizeof(value));
	TEST_NOT_NULL(list->next);
	return list;
}

int main(int argc, char **argv)
{
	dir_entry_xattr_t *xattr;
	struct stat sb;
	istream_t *fp;
	int ret;
	(void)argc; (void)argv;

	/* genereate some archive contents */
	memset(&sb, 0, sizeof(sb));
	sb.st_mode = S_IFDIR | 0755;
	sb.st_mtime = TIME_STAMP;
	ret = write_tar_header(&mem_stream, &sb, "dev/", NULL, NULL, 0);
	TEST_EQUAL_I(ret, 0);

	/* device files */
	sb.st_mode = S_IFCHR | 0620;
	sb.st_gid = 5;
	sb.st_rdev = makedev(4, 0);
	ret = write_tar_header(&mem_stream, &sb, "dev/tty0", NULL, NULL, 1);
	TEST_EQUAL_I(ret, 0);

	sb.st_rdev = makedev(4, 1);
	ret = write_tar_header(&mem_stream, &sb, "dev/tty1", NULL, NULL, 2);
	TEST_EQUAL_I(ret, 0);

	sb.st_rdev = makedev(4, 2);
	ret = write_tar_header(&mem_stream, &sb, "dev/tty2", NULL, NULL, 3);
	TEST_EQUAL_I(ret, 0);

	memset(&sb, 0, sizeof(sb));
	sb.st_mode = S_IFDIR | 0755;
	sb.st_mtime = TIME_STAMP;
	ret = write_tar_header(&mem_stream, &sb, "usr/", NULL, NULL, 4);
	TEST_EQUAL_I(ret, 0);

	ret = write_tar_header(&mem_stream, &sb, "usr/bin/", NULL, NULL, 5);
	TEST_EQUAL_I(ret, 0);

	/* sym link */
	sb.st_mode = S_IFLNK | 0777;
	sb.st_size = 7;
	ret = write_tar_header(&mem_stream, &sb, "bin", "usr/bin", NULL, 6);
	TEST_EQUAL_I(ret, 0);

	memset(&sb, 0, sizeof(sb));
	sb.st_mode = S_IFDIR | 0755;
	sb.st_mtime = TIME_STAMP;
	ret = write_tar_header(&mem_stream, &sb, "home/", NULL, NULL, 7);
	TEST_EQUAL_I(ret, 0);

	sb.st_mode = S_IFDIR | 0750;
	sb.st_uid = 1000;
	sb.st_gid = 1000;
	ret = write_tar_header(&mem_stream, &sb, "home/goliath/",
			       NULL, NULL, 8);
	TEST_EQUAL_I(ret, 0);

	/* regular file with actual content */
	sb.st_mode = S_IFREG | 0644;
	sb.st_size = 14;
	ret = write_tar_header(&mem_stream, &sb, "home/goliath/hello.txt",
			       NULL, NULL, 9);
	TEST_EQUAL_I(ret, 0);

	ret = ostream_append(&mem_stream, "Hello, World!\n", 14);
	TEST_EQUAL_I(ret, 0);
	ret = padd_file(&mem_stream, 14);
	TEST_EQUAL_I(ret, 0);

	/* hard link */
	ret = write_hard_link(&mem_stream, &sb, "home/goliath/world.txt",
			      "home/goliath/hello.txt", 10);
	TEST_EQUAL_I(ret, 0);

	/* something with xattrs */
	xattr = mkxattr_chain();
	sb.st_mode = S_IFREG | 0750;
	sb.st_size = 4;
	ret = write_tar_header(&mem_stream, &sb, "home/goliath/test.exe",
			       NULL, xattr, 11);
	TEST_EQUAL_I(ret, 0);
	dir_entry_xattr_list_free(xattr);

	ret = ostream_append(&mem_stream, ":-)\n", 4);
	TEST_EQUAL_I(ret, 0);
	ret = padd_file(&mem_stream, 4);
	TEST_EQUAL_I(ret, 0);

	/* now try something with a long name */
	memset(&sb, 0, sizeof(sb));
	sb.st_mode = S_IFREG | 0755;
	sb.st_mtime = TIME_STAMP;
	sb.st_size = 42;
	ret = write_tar_header(&mem_stream, &sb,
			       "mnt/windows_drive/C/Documents and Settings/"
			       "Joe Random User/My Documents/My Evil Plans/"
			       "file format nonsense/really long name.doc",
			       NULL, NULL, 12);
	TEST_EQUAL_I(ret, 0);

	ret = ostream_append(&mem_stream,
			     "Annoy people with really long file paths!\n",
			     42);
	TEST_EQUAL_I(ret, 0);
	ret = padd_file(&mem_stream, 42);
	TEST_EQUAL_I(ret, 0);

	/* compare with reference */
	TEST_EQUAL_UI(wr_offset, sizeof(wr_buffer));
	TEST_EQUAL_UI(sizeof(rd_buffer), sizeof(wr_buffer));

	fp = istream_open_file(STRVALUE(TESTPATH) "/" STRVALUE(TESTFILE));
	TEST_NOT_NULL(fp);

	ret = istream_read(fp, rd_buffer, sizeof(rd_buffer));
	TEST_ASSERT(ret > 0);
	TEST_EQUAL_UI(ret, sizeof(rd_buffer));

	ret = istream_read(fp, rd_buffer, sizeof(rd_buffer));
	TEST_EQUAL_I(ret, 0);

	sqfs_drop(fp);

	ret = memcmp(wr_buffer, rd_buffer, sizeof(rd_buffer));
	TEST_EQUAL_I(ret, 0);
	return EXIT_SUCCESS;
}
