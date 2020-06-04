/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * abi.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "sqfs/compressor.h"
#include "test.h"

int main(void)
{
	sqfs_compressor_config_t cfg;

	TEST_EQUAL_UI(sizeof(cfg.opt.gzip), sizeof(cfg.opt));
	TEST_EQUAL_UI(sizeof(cfg.opt.lzo), sizeof(cfg.opt));
	TEST_EQUAL_UI(sizeof(cfg.opt.xz), sizeof(cfg.opt));
	TEST_EQUAL_UI(sizeof(cfg.opt.padd0), sizeof(cfg.opt));

	return EXIT_SUCCESS;
}
