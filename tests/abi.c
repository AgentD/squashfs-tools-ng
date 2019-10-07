/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * abi.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "sqfs/compressor.h"

#include <assert.h>
#include <stdlib.h>

int main(void)
{
	sqfs_compressor_config_t cfg;

	assert(sizeof(cfg.opt.gzip) == sizeof(cfg.opt));
	assert(sizeof(cfg.opt.zstd) == sizeof(cfg.opt));
	assert(sizeof(cfg.opt.lzo) == sizeof(cfg.opt));
	assert(sizeof(cfg.opt.xz) == sizeof(cfg.opt));
	assert(sizeof(cfg.opt.padd0) == sizeof(cfg.opt));

	return EXIT_SUCCESS;
}
