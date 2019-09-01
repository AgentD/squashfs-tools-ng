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

int generic_write_options(int fd, const void *data, size_t size);

int generic_read_options(int fd, void *data, size_t size);

compressor_t *create_xz_compressor(const compressor_config_t *cfg);

compressor_t *create_gzip_compressor(const compressor_config_t *cfg);

compressor_t *create_lzo_compressor(const compressor_config_t *cfg);

compressor_t *create_lz4_compressor(const compressor_config_t *cfg);

compressor_t *create_zstd_compressor(const compressor_config_t *cfg);

#endif /* INTERNAL_H */
