/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef INTERNAL_H
#define INTERNAL_H

#include "compress.h"

compressor_t *create_lzma_compressor(bool compress, size_t block_size);

compressor_t *create_zlib_compressor(bool compress, size_t block_size);

#endif /* INTERNAL_H */
