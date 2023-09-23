/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * wrap.c
 *
 * Copyright (C) 2022 David Oberhollenzer <goliath@infraroot.at>
 */
#include "xfrm/compress.h"
#include "xfrm/stream.h"
#include "xfrm/wrap.h"
#include "util/test.h"
#include "sqfs/io.h"
#include "common.h"
#include "blob.h"

#if defined(DO_XZ)
#define mkdecompressor decompressor_stream_xz_create
#define mkcompressor compressor_stream_xz_create
#elif defined(DO_BZIP2)
#define mkdecompressor decompressor_stream_bzip2_create
#define mkcompressor compressor_stream_bzip2_create
#elif defined(DO_ZSTD)
#define mkdecompressor decompressor_stream_zstd_create
#define mkcompressor compressor_stream_zstd_create
#elif defined(DO_GZIP)
#define mkdecompressor decompressor_stream_gzip_create
#define mkcompressor compressor_stream_gzip_create
#endif

/*****************************************************************************/

static size_t mo_written = 0;
static sqfs_u8 mo_buffer[1024];
static bool mo_flushed = false;

static int mem_append(sqfs_ostream_t *strm, const void *data, size_t size);
static int mem_flush(sqfs_ostream_t *strm);

static sqfs_ostream_t mem_ostream = {
	{ 1, NULL, NULL, },
	mem_append,
	mem_flush,
	NULL,
};

static int mem_append(sqfs_ostream_t *strm, const void *data, size_t size)
{
	TEST_ASSERT(strm == &mem_ostream);
	TEST_ASSERT(size > 0);

	TEST_ASSERT(mo_written <= sizeof(mo_buffer));
	TEST_ASSERT(size <= (sizeof(mo_buffer) - mo_written));

	if (data == NULL) {
		memset(mo_buffer + mo_written, 0, size);
	} else {
		memcpy(mo_buffer + mo_written, data, size);
	}

	mo_written += size;
	return 0;
}

static int mem_flush(sqfs_ostream_t *strm)
{
	TEST_ASSERT(strm == &mem_ostream);
	TEST_ASSERT(!mo_flushed);
	mo_flushed = true;
	return 0;
}

/*****************************************************************************/

static void run_unpack_test(const void *blob, size_t size)
{
	sqfs_istream_t *istream, *mem_istream;
	xfrm_stream_t *xfrm;
	sqfs_s32 ret;
	size_t i;
	char c;

	mem_istream = istream_memory_create("memstream", 7, blob, size);
	TEST_NOT_NULL(mem_istream);

	xfrm = mkdecompressor();
	TEST_NOT_NULL(xfrm);
	TEST_EQUAL_UI(((sqfs_object_t *)xfrm)->refcount, 1);
	TEST_EQUAL_UI(((sqfs_object_t *)mem_istream)->refcount, 1);

	istream = istream_xfrm_create(mem_istream, xfrm);

	TEST_NOT_NULL(istream);
	TEST_EQUAL_UI(((sqfs_object_t *)istream)->refcount, 1);
	TEST_EQUAL_UI(((sqfs_object_t *)xfrm)->refcount, 2);
	TEST_EQUAL_UI(((sqfs_object_t *)mem_istream)->refcount, 2);

	for (i = 0; i < (sizeof(orig) - 1); ++i) {
		ret = sqfs_istream_read(istream, &c, 1);
		TEST_EQUAL_I(ret, 1);
		TEST_EQUAL_I(c, orig[i]);
	}

	ret = sqfs_istream_read(istream, &c, 1);
	TEST_EQUAL_I(ret, 0);

	ret = sqfs_istream_read(istream, &c, 1);
	TEST_EQUAL_I(ret, 0);

	sqfs_drop(istream);
	TEST_EQUAL_UI(((sqfs_object_t *)mem_istream)->refcount, 1);
	TEST_EQUAL_UI(((sqfs_object_t *)xfrm)->refcount, 1);
	sqfs_drop(xfrm);
	sqfs_drop(mem_istream);
}

static void run_pack_test(void)
{
	sqfs_ostream_t *ostream;
	xfrm_stream_t *xfrm;
	size_t i;
	int ret;

	mo_written = 0;
	mo_flushed = false;

	xfrm = mkcompressor(NULL);
	TEST_NOT_NULL(xfrm);
	TEST_EQUAL_UI(((sqfs_object_t *)xfrm)->refcount, 1);
	TEST_EQUAL_UI(((sqfs_object_t *)&mem_ostream)->refcount, 1);

	ostream = ostream_xfrm_create(&mem_ostream, xfrm);

	TEST_NOT_NULL(ostream);
	TEST_EQUAL_UI(((sqfs_object_t *)ostream)->refcount, 1);
	TEST_EQUAL_UI(((sqfs_object_t *)xfrm)->refcount, 2);
	TEST_EQUAL_UI(((sqfs_object_t *)&mem_ostream)->refcount, 2);

	for (i = 0; i < (sizeof(orig) - 1); ++i) {
		ret = ostream->append(ostream, orig + i, 1);
		TEST_EQUAL_I(ret, 0);
	}

	ret = ostream->flush(ostream);
	TEST_EQUAL_I(ret, 0);

	TEST_ASSERT(mo_flushed);
	TEST_ASSERT(mo_written < sizeof(orig));
	ret = memcmp(mo_buffer, orig, mo_written);
	TEST_ASSERT(ret != 0);

	sqfs_drop(ostream);
	TEST_EQUAL_UI(((sqfs_object_t *)&mem_ostream)->refcount, 1);
	TEST_EQUAL_UI(((sqfs_object_t *)xfrm)->refcount, 1);
	sqfs_drop(xfrm);
}

int main(int argc, char **argv)
{
	(void)argc; (void)argv;

	/* normal stream */
	run_unpack_test(blob_in, sizeof(blob_in));

	/* concatenated streams */
#if !defined(DO_GZIP)
	run_unpack_test(blob_in_concat, sizeof(blob_in_concat));
#else
	(void)blob_in_concat;
#endif
	/* compress */
	run_pack_test();

	/* restore from compressed */
	run_unpack_test(mo_buffer, mo_written);
	return EXIT_SUCCESS;
}
