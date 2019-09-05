/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * table.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef SQFS_TABLE_H
#define SQFS_TABLE_H

#include "sqfs/compress.h"
#include "sqfs/super.h"

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
  Convenience function for writing meta data to a SquashFS image

  This function internally creates a meta data writer and writes the given
  'data' blob with 'table_size' bytes to disk, neatly partitioned into meta
  data blocks. For each meta data block, it remembers the 64 bit start address,
  writes out all addresses to an uncompressed list and returns the location
  where the address list starts in 'start'.

  Returns 0 on success. Internally prints error messages to stderr.
 */
int sqfs_write_table(int outfd, sqfs_super_t *super, sqfs_compressor_t *cmp,
		     const void *data, size_t table_size, uint64_t *start);

void *sqfs_read_table(int fd, sqfs_compressor_t *cmp, size_t table_size,
		      uint64_t location, uint64_t lower_limit,
		      uint64_t upper_limit);

#ifdef __cplusplus
}
#endif

#endif /* SQFS_TABLE_H */
