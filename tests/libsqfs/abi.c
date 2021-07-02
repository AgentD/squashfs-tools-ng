/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * abi.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "sqfs/block_processor.h"
#include "sqfs/compressor.h"
#include "sqfs/block.h"
#include "../test.h"

#include <stddef.h>

static void test_compressor_opt_struct(void)
{
	sqfs_compressor_config_t cfg;

	TEST_EQUAL_UI(sizeof(cfg.id), sizeof(sqfs_u16));
	TEST_EQUAL_UI(sizeof(cfg.flags), sizeof(sqfs_u16));
	TEST_EQUAL_UI(sizeof(cfg.block_size), sizeof(sqfs_u32));
	TEST_EQUAL_UI(sizeof(cfg.level), sizeof(sqfs_u32));
	TEST_EQUAL_UI(sizeof(cfg.opt), (2 * sizeof(sqfs_u64)));

	TEST_EQUAL_UI(sizeof(cfg.opt.gzip), sizeof(cfg.opt));
	TEST_EQUAL_UI(sizeof(cfg.opt.lzo), sizeof(cfg.opt));
	TEST_EQUAL_UI(sizeof(cfg.opt.xz), sizeof(cfg.opt));
	TEST_EQUAL_UI(sizeof(cfg.opt.padd0), sizeof(cfg.opt));

	TEST_EQUAL_UI(offsetof(sqfs_compressor_config_t, id), 0);
	TEST_EQUAL_UI(offsetof(sqfs_compressor_config_t, flags),
		      sizeof(sqfs_u16));
	TEST_EQUAL_UI(offsetof(sqfs_compressor_config_t, block_size),
		      sizeof(sqfs_u32));
	TEST_EQUAL_UI(offsetof(sqfs_compressor_config_t, level),
		      (2 * sizeof(sqfs_u32)));
	TEST_EQUAL_UI(offsetof(sqfs_compressor_config_t, opt),
		      (4 * sizeof(sqfs_u32)));
}

static const char *names[] = {
	[SQFS_COMP_GZIP] = "gzip",
	[SQFS_COMP_LZMA] = "lzma",
	[SQFS_COMP_LZO] = "lzo",
	[SQFS_COMP_XZ] = "xz",
	[SQFS_COMP_LZ4] = "lz4",
	[SQFS_COMP_ZSTD] = "zstd",
};

static void test_compressor_names(void)
{
	const char *str;
	int i, id;

	for (i = SQFS_COMP_MIN; i <= SQFS_COMP_MAX; ++i) {
		str = sqfs_compressor_name_from_id(i);
		TEST_STR_EQUAL(str, names[i]);

		id = sqfs_compressor_id_from_name(str);
		TEST_EQUAL_I(id, i);
	}
}

static void test_blockproc_stats(void)
{
	sqfs_block_processor_stats_t stats;

	TEST_ASSERT(sizeof(stats) >= (8 * sizeof(sqfs_u64)));

	TEST_EQUAL_UI(offsetof(sqfs_block_processor_stats_t, size), 0);
	TEST_EQUAL_UI(offsetof(sqfs_block_processor_stats_t,
			       input_bytes_read), sizeof(sqfs_u64));
	TEST_EQUAL_UI(offsetof(sqfs_block_processor_stats_t,
			       output_bytes_generated), 2 * sizeof(sqfs_u64));
	TEST_EQUAL_UI(offsetof(sqfs_block_processor_stats_t,
			       data_block_count), 3 * sizeof(sqfs_u64));
	TEST_EQUAL_UI(offsetof(sqfs_block_processor_stats_t,
			       frag_block_count), 4 * sizeof(sqfs_u64));
	TEST_EQUAL_UI(offsetof(sqfs_block_processor_stats_t,
			       sparse_block_count), 5 * sizeof(sqfs_u64));
	TEST_EQUAL_UI(offsetof(sqfs_block_processor_stats_t,
			       total_frag_count), 6 * sizeof(sqfs_u64));
	TEST_EQUAL_UI(offsetof(sqfs_block_processor_stats_t,
			       actual_frag_count), 7 * sizeof(sqfs_u64));

	TEST_EQUAL_UI(sizeof(stats.size), sizeof(size_t));
	TEST_EQUAL_UI(sizeof(stats.input_bytes_read), sizeof(sqfs_u64));
	TEST_EQUAL_UI(sizeof(stats.output_bytes_generated), sizeof(sqfs_u64));
	TEST_EQUAL_UI(sizeof(stats.data_block_count), sizeof(sqfs_u64));
	TEST_EQUAL_UI(sizeof(stats.frag_block_count), sizeof(sqfs_u64));
	TEST_EQUAL_UI(sizeof(stats.sparse_block_count), sizeof(sqfs_u64));
	TEST_EQUAL_UI(sizeof(stats.total_frag_count), sizeof(sqfs_u64));
	TEST_EQUAL_UI(sizeof(stats.actual_frag_count), sizeof(sqfs_u64));
}

static void test_blockproc_desc(void)
{
	sqfs_block_processor_desc_t desc;

	TEST_ASSERT(sizeof(desc) >= (4 * sizeof(sqfs_u32) +
				     5 * sizeof(void *)));

	TEST_EQUAL_UI(sizeof(desc.size), sizeof(sqfs_u32));
	TEST_EQUAL_UI(sizeof(desc.max_block_size), sizeof(sqfs_u32));
	TEST_EQUAL_UI(sizeof(desc.num_workers), sizeof(sqfs_u32));
	TEST_EQUAL_UI(sizeof(desc.max_backlog), sizeof(sqfs_u32));
	TEST_EQUAL_UI(sizeof(desc.cmp), sizeof(void *));
	TEST_EQUAL_UI(sizeof(desc.wr), sizeof(void *));
	TEST_EQUAL_UI(sizeof(desc.tbl), sizeof(void *));
	TEST_EQUAL_UI(sizeof(desc.file), sizeof(void *));
	TEST_EQUAL_UI(sizeof(desc.uncmp), sizeof(void *));

	TEST_EQUAL_UI(offsetof(sqfs_block_processor_desc_t, size), 0);
	TEST_EQUAL_UI(offsetof(sqfs_block_processor_desc_t, max_block_size),
		      sizeof(sqfs_u32));
	TEST_EQUAL_UI(offsetof(sqfs_block_processor_desc_t, num_workers),
		      (2 * sizeof(sqfs_u32)));
	TEST_EQUAL_UI(offsetof(sqfs_block_processor_desc_t, max_backlog),
		      (3 * sizeof(sqfs_u32)));
	TEST_EQUAL_UI(offsetof(sqfs_block_processor_desc_t, cmp),
		      (4 * sizeof(sqfs_u32)));
	TEST_EQUAL_UI(offsetof(sqfs_block_processor_desc_t, wr),
		      (4 * sizeof(sqfs_u32) + sizeof(void *)));
	TEST_EQUAL_UI(offsetof(sqfs_block_processor_desc_t, tbl),
		      (4 * sizeof(sqfs_u32) + 2 * sizeof(void *)));
	TEST_EQUAL_UI(offsetof(sqfs_block_processor_desc_t, file),
		      (4 * sizeof(sqfs_u32) + 3 * sizeof(void *)));
	TEST_EQUAL_UI(offsetof(sqfs_block_processor_desc_t, uncmp),
		      (4 * sizeof(sqfs_u32) + 4 * sizeof(void *)));
}

int main(void)
{
	test_compressor_opt_struct();
	test_compressor_names();
	test_blockproc_stats();
	test_blockproc_desc();
	return EXIT_SUCCESS;
}
