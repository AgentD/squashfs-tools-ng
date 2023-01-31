/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * compress.c
 *
 * Copyright (C) 2022 David Oberhollenzer <goliath@infraroot.at>
 */
#include "xfrm/compress.h"
#include "xfrm/stream.h"
#include "util/test.h"

static const char text[] =
"Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod\n"
"tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam,\n"
"quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo\n"
"consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse\n"
"cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non\n"
"proident, sunt in culpa qui officia deserunt mollit anim id est laborum.\n";

static sqfs_u8 buffer_cmp[1024];
static sqfs_u8 buffer_plain[1024];
static sqfs_u8 ref_cmp[1024];
static size_t ref_size;

#if defined(DO_BZIP2)
#define	mkdecompressor decompressor_stream_bzip2_create
#define mkcompressor compressor_stream_bzip2_create

static sqfs_u8 magic[3] = "BZh";
static int comp_id = XFRM_COMPRESSOR_BZIP2;
static const char *comp_name = "bzip2";
#elif defined(DO_XZ)
#define	mkdecompressor decompressor_stream_xz_create
#define mkcompressor compressor_stream_xz_create

static sqfs_u8 magic[6] = "\xFD" "7zXZ";
static int comp_id = XFRM_COMPRESSOR_XZ;
static const char *comp_name = "xz";
#elif defined(DO_GZIP)
#define	mkdecompressor decompressor_stream_gzip_create
#define mkcompressor compressor_stream_gzip_create

static sqfs_u8 magic[3] = "\x1F\x8B\x08";
static int comp_id = XFRM_COMPRESSOR_GZIP;
static const char *comp_name = "gzip";
#elif defined(DO_ZSTD)
#define	mkdecompressor decompressor_stream_zstd_create
#define mkcompressor compressor_stream_zstd_create

static sqfs_u8 magic[4] = "\x28\xB5\x2F\xFD";
static int comp_id = XFRM_COMPRESSOR_ZSTD;
static const char *comp_name = "zstd";
#else
#error build configuration broken
#endif

int main(int argc, char **argv)
{
	sqfs_u32 in_diff = 0, out_diff = 0;
	xfrm_stream_t *xfrm;
	const char *str;
	size_t size;
	int ret;
	(void)argc; (void)argv;

	/* generic name/ID API */
	ret = xfrm_compressor_id_from_name(comp_name);
	TEST_EQUAL_I(ret, comp_id);

	str = xfrm_compressor_name_from_id(ret);
	TEST_STR_EQUAL(str, comp_name);

	/* compress the original text */
	xfrm = mkcompressor(NULL);
	TEST_NOT_NULL(xfrm);
	TEST_EQUAL_UI(((sqfs_object_t *)xfrm)->refcount, 1);

	ret = xfrm->process_data(xfrm, text, sizeof(text),
				 buffer_cmp, sizeof(buffer_cmp),
				 &in_diff, &out_diff, XFRM_STREAM_FLUSH_FULL);
	TEST_EQUAL_I(ret, XFRM_STREAM_END);

	TEST_EQUAL_UI(in_diff, sizeof(text));
	TEST_ASSERT(out_diff > 0);
	TEST_ASSERT(out_diff < sizeof(text));

	sqfs_drop(xfrm);

	size = out_diff;
	in_diff = out_diff = 0;

	memcpy(ref_cmp, buffer_cmp, size);
	ref_size = size;

	/* check if it has the expected magic number */
	TEST_ASSERT(size >= sizeof(magic));
	ret = memcmp(buffer_cmp, magic, sizeof(magic));
	TEST_EQUAL_I(ret, 0);

	/* check if the auto detection correctly identifies it */
	ret = xfrm_compressor_id_from_magic(buffer_cmp, size);
	TEST_EQUAL_I(ret, comp_id);

	ret = xfrm_compressor_id_from_magic(text, sizeof(text));
	TEST_EQUAL_I(ret, -1);

	/* unpack the compressed text and compare to the original */
	xfrm = mkdecompressor();
	TEST_NOT_NULL(xfrm);
	TEST_EQUAL_UI(((sqfs_object_t *)xfrm)->refcount, 1);

	ret = xfrm->process_data(xfrm, buffer_cmp, size,
				 buffer_plain, sizeof(buffer_plain),
				 &in_diff, &out_diff, 0);
	TEST_ASSERT(ret == XFRM_STREAM_END || ret == XFRM_STREAM_OK);

	TEST_EQUAL_UI(in_diff, size);
	TEST_EQUAL_UI(out_diff, sizeof(text));
	ret = memcmp(buffer_plain, text, out_diff);
	TEST_EQUAL_I(ret, 0);

	sqfs_drop(xfrm);
	in_diff = out_diff = 0;

	/* retry packing but create the compressor using the ID */
	memset(buffer_cmp, 0, sizeof(buffer_cmp));
	memset(buffer_plain, 0, sizeof(buffer_plain));

	xfrm = compressor_stream_create(comp_id, NULL);
	TEST_NOT_NULL(xfrm);
	TEST_EQUAL_UI(((sqfs_object_t *)xfrm)->refcount, 1);

	ret = xfrm->process_data(xfrm, text, sizeof(text),
				 buffer_cmp, sizeof(buffer_cmp),
				 &in_diff, &out_diff, XFRM_STREAM_FLUSH_FULL);
	TEST_EQUAL_I(ret, XFRM_STREAM_END);

	TEST_EQUAL_UI(in_diff, sizeof(text));
	TEST_EQUAL_UI(out_diff, ref_size);

	sqfs_drop(xfrm);
	size = out_diff;
	in_diff = out_diff = 0;

	/* make sure we got an identical result */
	ret = memcmp(ref_cmp, buffer_cmp, size);
	TEST_EQUAL_I(ret, 0);

	/* decompress it using ID */
	xfrm = decompressor_stream_create(comp_id);
	TEST_NOT_NULL(xfrm);
	TEST_EQUAL_UI(((sqfs_object_t *)xfrm)->refcount, 1);

	ret = xfrm->process_data(xfrm, buffer_cmp, size,
				 buffer_plain, sizeof(buffer_plain),
				 &in_diff, &out_diff, 0);
	TEST_ASSERT(ret == XFRM_STREAM_END || ret == XFRM_STREAM_OK);

	TEST_EQUAL_UI(in_diff, size);
	TEST_EQUAL_UI(out_diff, sizeof(text));
	ret = memcmp(buffer_plain, text, out_diff);
	TEST_EQUAL_I(ret, 0);

	sqfs_drop(xfrm);
	return EXIT_SUCCESS;
}
