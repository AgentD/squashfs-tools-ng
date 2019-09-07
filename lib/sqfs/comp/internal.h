/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * internal.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef INTERNAL_H
#define INTERNAL_H

#include "config.h"

#include "sqfs/predef.h"
#include "sqfs/compress.h"
#include "sqfs/error.h"
#include "util.h"

SQFS_INTERNAL
int sqfs_generic_write_options(int fd, const void *data, size_t size);

SQFS_INTERNAL
int sqfs_generic_read_options(int fd, void *data, size_t size);

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

#endif /* INTERNAL_H */
