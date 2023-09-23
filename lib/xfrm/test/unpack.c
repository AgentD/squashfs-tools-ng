/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * unpack.c
 *
 * Copyright (C) 2022 David Oberhollenzer <goliath@infraroot.at>
 */
#include "xfrm/compress.h"
#include "xfrm/stream.h"
#include "util/test.h"
#include "blob.h"

#if defined(DO_XZ)
static size_t in_stop = 244;
static size_t out_stop = 221;

#define mkdecompressor decompressor_stream_xz_create
#elif defined(DO_BZIP2)
static size_t in_stop = 186;
static size_t out_stop = 221;

#define mkdecompressor decompressor_stream_bzip2_create
#elif defined(DO_ZSTD)
static size_t in_stop = 319;
static size_t out_stop = 446;

#define mkdecompressor decompressor_stream_zstd_create
#elif defined(DO_GZIP)
#define mkdecompressor decompressor_stream_gzip_create
#endif

int main(int argc, char **argv)
{
	sqfs_u32 in_diff = 0, out_diff = 0;
	xfrm_stream_t *xfrm;
	char buffer[1024];
	int ret;
	(void)argc; (void)argv;

	/* normal stream */
	xfrm = mkdecompressor();
	TEST_NOT_NULL(xfrm);
	TEST_EQUAL_UI(((sqfs_object_t *)xfrm)->refcount, 1);

	ret = xfrm->process_data(xfrm, blob_in, sizeof(blob_in),
				 buffer, sizeof(buffer),
				 &in_diff, &out_diff, XFRM_STREAM_FLUSH_FULL);
	TEST_EQUAL_I(ret, XFRM_STREAM_END);

	TEST_EQUAL_UI(in_diff, sizeof(blob_in));
	TEST_EQUAL_UI(out_diff, sizeof(orig) - 1);
	ret = memcmp(buffer, orig, out_diff);
	TEST_EQUAL_I(ret, 0);

	sqfs_drop(xfrm);

	/* concatenated streams */
#if !defined(DO_GZIP)
	xfrm = mkdecompressor();
	TEST_NOT_NULL(xfrm);
	TEST_EQUAL_UI(((sqfs_object_t *)xfrm)->refcount, 1);

	in_diff = 0;
	out_diff = 0;

	ret = xfrm->process_data(xfrm, blob_in_concat, sizeof(blob_in_concat),
				 buffer, sizeof(buffer),
				 &in_diff, &out_diff, XFRM_STREAM_FLUSH_FULL);
	TEST_EQUAL_I(ret, XFRM_STREAM_END);

	TEST_EQUAL_UI(in_diff, in_stop);
	TEST_EQUAL_UI(out_diff, out_stop);
	ret = memcmp(buffer, orig, out_diff);
	TEST_EQUAL_I(ret, 0);

	ret = xfrm->process_data(xfrm, blob_in_concat + in_diff,
				 sizeof(blob_in_concat) - in_diff,
				 buffer + out_diff, sizeof(buffer) - out_diff,
				 &in_diff, &out_diff, XFRM_STREAM_FLUSH_FULL);
	TEST_EQUAL_I(ret, XFRM_STREAM_END);

	TEST_EQUAL_UI(in_diff, sizeof(blob_in_concat));
	TEST_EQUAL_UI(out_diff, sizeof(orig) - 1);
	ret = memcmp(buffer, orig, out_diff);
	TEST_EQUAL_I(ret, 0);

	sqfs_drop(xfrm);
#else
	(void)blob_in_concat;
#endif
	return EXIT_SUCCESS;
}
