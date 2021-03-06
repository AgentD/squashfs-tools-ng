/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * xattr_writer.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "compat.h"
#include "../test.h"

#include "sqfs/xattr_writer.h"
#include "sqfs/compressor.h"
#include "sqfs/xattr.h"
#include "sqfs/error.h"
#include "sqfs/super.h"
#include "sqfs/io.h"

static sqfs_u8 file_data[1024];
static size_t file_used = 0;

static int dummy_write_at(sqfs_file_t *file, sqfs_u64 offset,
			  const void *buffer, size_t size)
{
	(void)file;

	if (offset >= sizeof(file_data))
		return SQFS_ERROR_OUT_OF_BOUNDS;

	if (size > (sizeof(file_data) - offset))
		return SQFS_ERROR_OUT_OF_BOUNDS;

	if (offset > file_used)
		memset(file_data + file_used, 0, offset - file_used);

	if ((offset + size) > file_used)
		file_used = offset + size;

	memcpy(file_data + offset, buffer, size);
	return 0;
}

static sqfs_u64 dummy_get_size(const sqfs_file_t *file)
{
	(void)file;
	return file_used;
}

static sqfs_s32 dummy_compress(sqfs_compressor_t *cmp, const sqfs_u8 *in,
			       sqfs_u32 size, sqfs_u8 *out, sqfs_u32 outsize)
{
	(void)cmp;
	memcpy(out, in, outsize < size ? outsize : size);
	return 0;
}

static sqfs_file_t dummy_file = {
	{ NULL, NULL },
	NULL,
	dummy_write_at,
	dummy_get_size,
	NULL,
};

static sqfs_compressor_t dummy_compressor = {
	{ NULL, NULL },
	NULL,
	NULL,
	NULL,
	dummy_compress,
};

/*****************************************************************************/

int main(void)
{
	size_t offset, ool_value_offset, id_offset;
	sqfs_xattr_id_table_t idtbl;
	sqfs_xattr_writer_t *xwr;
	sqfs_xattr_value_t value;
	sqfs_xattr_entry_t key;
	sqfs_xattr_id_t desc;
	sqfs_super_t super;
	char strbuf[32];
	sqfs_u16 hdr;
	sqfs_u64 ref;
	sqfs_u32 id;
	int ret;

	/* setup */
	xwr = sqfs_xattr_writer_create(0);
	TEST_NOT_NULL(xwr);

	/* record a block of key/value pairs */
	ret = sqfs_xattr_writer_begin(xwr, 0);
	TEST_EQUAL_I(ret, 0);

	ret = sqfs_xattr_writer_add(xwr, "user.foobar", "test", 4);
	TEST_EQUAL_I(ret, 0);

	ret = sqfs_xattr_writer_add(xwr, "security.selinux", "Xwhatever", 9);
	TEST_EQUAL_I(ret, 0);

	ret = sqfs_xattr_writer_end(xwr, &id);
	TEST_EQUAL_I(ret, 0);
	TEST_EQUAL_UI(id, 0);

	/* record a second, different block */
	ret = sqfs_xattr_writer_begin(xwr, 0);
	TEST_EQUAL_I(ret, 0);

	ret = sqfs_xattr_writer_add(xwr, "user.foobar", "bla", 3);
	TEST_EQUAL_I(ret, 0);

	ret = sqfs_xattr_writer_add(xwr, "security.selinux", "blub", 4);
	TEST_EQUAL_I(ret, 0);

	ret = sqfs_xattr_writer_end(xwr, &id);
	TEST_EQUAL_I(ret, 0);
	TEST_EQUAL_UI(id, 1);

	/* same as first block after sorting and gets the same ID */
	ret = sqfs_xattr_writer_begin(xwr, 0);
	TEST_EQUAL_I(ret, 0);

	ret = sqfs_xattr_writer_add(xwr, "security.selinux", "Xwhatever", 9);
	TEST_EQUAL_I(ret, 0);

	ret = sqfs_xattr_writer_add(xwr, "user.foobar", "test", 4);
	TEST_EQUAL_I(ret, 0);

	ret = sqfs_xattr_writer_end(xwr, &id);
	TEST_EQUAL_I(ret, 0);
	TEST_EQUAL_UI(id, 0);

	/* the third assignment overwrites the first, making
	   the block identical to the second one */
	ret = sqfs_xattr_writer_begin(xwr, 0);
	TEST_EQUAL_I(ret, 0);

	ret = sqfs_xattr_writer_add(xwr, "user.foobar", "mimimi", 6);
	TEST_EQUAL_I(ret, 0);

	ret = sqfs_xattr_writer_add(xwr, "security.selinux", "blub", 4);
	TEST_EQUAL_I(ret, 0);

	ret = sqfs_xattr_writer_add(xwr, "user.foobar", "bla", 3);
	TEST_EQUAL_I(ret, 0);

	ret = sqfs_xattr_writer_end(xwr, &id);
	TEST_EQUAL_I(ret, 0);
	TEST_EQUAL_UI(id, 1);

	/* add another block with the same value, so it gets stored OOL */
	ret = sqfs_xattr_writer_begin(xwr, 0);
	TEST_EQUAL_I(ret, 0);

	ret = sqfs_xattr_writer_add(xwr, "security.selinux", "Xwhatever", 9);
	TEST_EQUAL_I(ret, 0);

	ret = sqfs_xattr_writer_end(xwr, &id);
	TEST_EQUAL_I(ret, 0);
	TEST_EQUAL_UI(id, 2);

	/* serialize */
	sqfs_super_init(&super, 131072, 0, SQFS_COMP_GZIP);
	ret = sqfs_xattr_writer_flush(xwr, &dummy_file, &super,
				      &dummy_compressor);
	TEST_EQUAL_I(ret, 0);

	TEST_EQUAL_UI(file_used, 177);

	/* meta data block holding the key-value-pairs */
	memcpy(&hdr, file_data, sizeof(hdr));
	hdr = le16toh(hdr);
	TEST_EQUAL_UI(hdr, (0x8000 | 101));
	offset = 2;

	memcpy(&key, file_data + offset, sizeof(key));
	key.type = le16toh(key.type);
	key.size = le16toh(key.size);
	offset += sizeof(key);
	TEST_EQUAL_UI(key.type, SQFS_XATTR_USER);
	TEST_EQUAL_UI(key.size, 6);
	memset(strbuf, '\0', sizeof(strbuf));
	memcpy(strbuf, file_data + offset, key.size);
	TEST_STR_EQUAL(strbuf, "foobar");
	offset += key.size;

	memcpy(&value, file_data + offset, sizeof(value));
	value.size = le32toh(value.size);
	offset += sizeof(value);
	TEST_EQUAL_UI(value.size, 4);
	memset(strbuf, '\0', sizeof(strbuf));
	memcpy(strbuf, file_data + offset, value.size);
	TEST_STR_EQUAL(strbuf, "test");
	offset += value.size;

	memcpy(&key, file_data + offset, sizeof(key));
	key.type = le16toh(key.type);
	key.size = le16toh(key.size);
	offset += sizeof(key);
	TEST_EQUAL_UI(key.type, SQFS_XATTR_SECURITY);
	TEST_EQUAL_UI(key.size, 7);
	memset(strbuf, '\0', sizeof(strbuf));
	memcpy(strbuf, file_data + offset, key.size);
	TEST_STR_EQUAL(strbuf, "selinux");
	offset += key.size;

	ool_value_offset = offset;

	memcpy(&value, file_data + offset, sizeof(value));
	value.size = le32toh(value.size);
	offset += sizeof(value);
	TEST_EQUAL_UI(value.size, 9);
	memset(strbuf, '\0', sizeof(strbuf));
	memcpy(strbuf, file_data + offset, value.size);
	TEST_STR_EQUAL(strbuf, "Xwhatever");
	offset += value.size;

	memcpy(&key, file_data + offset, sizeof(key));
	key.type = le16toh(key.type);
	key.size = le16toh(key.size);
	offset += sizeof(key);
	TEST_EQUAL_UI(key.type, SQFS_XATTR_USER);
	TEST_EQUAL_UI(key.size, 6);
	memset(strbuf, '\0', sizeof(strbuf));
	memcpy(strbuf, file_data + offset, key.size);
	TEST_STR_EQUAL(strbuf, "foobar");
	offset += key.size;

	memcpy(&value, file_data + offset, sizeof(value));
	value.size = le32toh(value.size);
	offset += sizeof(value);
	TEST_EQUAL_UI(value.size, 3);
	memset(strbuf, '\0', sizeof(strbuf));
	memcpy(strbuf, file_data + offset, value.size);
	TEST_STR_EQUAL(strbuf, "bla");
	offset += value.size;

	memcpy(&key, file_data + offset, sizeof(key));
	key.type = le16toh(key.type);
	key.size = le16toh(key.size);
	offset += sizeof(key);
	TEST_EQUAL_UI(key.type, SQFS_XATTR_SECURITY);
	TEST_EQUAL_UI(key.size, 7);
	memset(strbuf, '\0', sizeof(strbuf));
	memcpy(strbuf, file_data + offset, key.size);
	TEST_STR_EQUAL(strbuf, "selinux");
	offset += key.size;

	memcpy(&value, file_data + offset, sizeof(value));
	value.size = le32toh(value.size);
	offset += sizeof(value);
	TEST_EQUAL_UI(value.size, 4);
	memset(strbuf, '\0', sizeof(strbuf));
	memcpy(strbuf, file_data + offset, value.size);
	TEST_STR_EQUAL(strbuf, "blub");
	offset += value.size;

	memcpy(&key, file_data + offset, sizeof(key));
	key.type = le16toh(key.type);
	key.size = le16toh(key.size);
	offset += sizeof(key);
	TEST_EQUAL_UI(key.type, (SQFS_XATTR_SECURITY | SQFS_XATTR_FLAG_OOL));
	TEST_EQUAL_UI(key.size, 7);
	memset(strbuf, '\0', sizeof(strbuf));
	memcpy(strbuf, file_data + offset, key.size);
	TEST_STR_EQUAL(strbuf, "selinux");
	offset += key.size;

	memcpy(&value, file_data + offset, sizeof(value));
	value.size = le32toh(value.size);
	offset += sizeof(value);
	TEST_EQUAL_UI(value.size, 8);
	memcpy(&ref, file_data + offset, sizeof(ref));
	ref = le64toh(ref);
	TEST_EQUAL_UI(ref, (ool_value_offset - 2));
	offset += value.size;

	/* meta data block holding the ID descriptions */
	id_offset = offset;

	memcpy(&hdr, file_data + offset, sizeof(hdr));
	TEST_EQUAL_UI(le16toh(hdr), (0x8000 | (16 * 3)));
	offset += sizeof(hdr);

	memcpy(&desc, file_data + offset, sizeof(desc));
	TEST_EQUAL_UI(le64toh(desc.xattr), 0);
	TEST_EQUAL_UI(le32toh(desc.count), 2);
	TEST_EQUAL_UI(le32toh(desc.size), 42);
	offset += sizeof(desc);

	memcpy(&desc, file_data + offset, sizeof(desc));
	TEST_EQUAL_UI(le64toh(desc.xattr), 42);
	TEST_EQUAL_UI(le32toh(desc.count), 2);
	TEST_EQUAL_UI(le32toh(desc.size), 36);
	offset += sizeof(desc);

	memcpy(&desc, file_data + offset, sizeof(desc));
	TEST_EQUAL_UI(le64toh(desc.xattr), 78);
	TEST_EQUAL_UI(le32toh(desc.count), 1);
	TEST_EQUAL_UI(le32toh(desc.size), 23);
	offset += sizeof(desc);

	/* the xattr table itself */
	TEST_EQUAL_UI(super.xattr_id_table_start, offset);

	memcpy(&idtbl, file_data + offset, sizeof(idtbl));
	TEST_EQUAL_UI(le64toh(idtbl.xattr_table_start), 0);
	TEST_EQUAL_UI(le32toh(idtbl.xattr_ids), 3);
	offset += sizeof(idtbl);

	memcpy(&ref, file_data + offset, sizeof(ref));
	TEST_EQUAL_UI(le64toh(ref), id_offset);
	offset += sizeof(ref);

	TEST_EQUAL_UI(offset, file_used);

	/* cleanup */
	sqfs_destroy(xwr);
	return EXIT_SUCCESS;
}
