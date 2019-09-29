/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * internal.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef INTERNAL_H
#define INTERNAL_H

#include "config.h"

#include "sqfs/predef.h"
#include "sqfs/compressor.h"
#include "sqfs/error.h"
#include "sqfs/block.h"
#include "sqfs/io.h"
#include "util.h"

SQFS_INTERNAL
int sqfs_generic_write_options(sqfs_file_t *file, const void *data,
			       size_t size);

SQFS_INTERNAL
int sqfs_generic_read_options(sqfs_file_t *file, void *data, size_t size);

SQFS_INTERNAL
sqfs_compressor_t *xz_compressor_create(const sqfs_compressor_config_t *cfg);

SQFS_INTERNAL
sqfs_compressor_t *gzip_compressor_create(const sqfs_compressor_config_t *cfg);

SQFS_INTERNAL
sqfs_compressor_t *lzo_compressor_create(const sqfs_compressor_config_t *cfg);

SQFS_INTERNAL
sqfs_compressor_t *lz4_compressor_create(const sqfs_compressor_config_t *cfg);

SQFS_INTERNAL
sqfs_compressor_t *zstd_compressor_create(const sqfs_compressor_config_t *cfg);

SQFS_INTERNAL
sqfs_compressor_t *lzma_compressor_create(const sqfs_compressor_config_t *cfg);

#endif /* INTERNAL_H */
