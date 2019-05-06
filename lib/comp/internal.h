/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef INTERNAL_H
#define INTERNAL_H

#include "compress.h"

int generic_write_options(int fd, const void *data, size_t size);

int generic_read_options(int fd, void *data, size_t size);

compressor_t *create_xz_compressor(bool compress, size_t block_size);

compressor_t *create_gzip_compressor(bool compress, size_t block_size);

compressor_t *create_lzo_compressor(bool compress, size_t block_size);

compressor_t *create_lz4_compressor(bool compress, size_t block_size);

#endif /* INTERNAL_H */
