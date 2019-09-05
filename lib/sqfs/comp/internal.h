/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * internal.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef INTERNAL_H
#define INTERNAL_H

#include "config.h"

#include "sqfs/compress.h"
#include "util.h"

int sqfs_generic_write_options(int fd, const void *data, size_t size);

int sqfs_generic_read_options(int fd, void *data, size_t size);

sqfs_compressor_t *xz_compressor_create(const sqfs_compressor_config_t *cfg);

sqfs_compressor_t *gzip_compressor_create(const sqfs_compressor_config_t *cfg);

sqfs_compressor_t *lzo_compressor_create(const sqfs_compressor_config_t *cfg);

sqfs_compressor_t *lz4_compressor_create(const sqfs_compressor_config_t *cfg);

sqfs_compressor_t *zstd_compressor_create(const sqfs_compressor_config_t *cfg);

#endif /* INTERNAL_H */
