/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * table.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef SQFS_TABLE_H
#define SQFS_TABLE_H

#include "sqfs/predef.h"

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
SQFS_API int sqfs_write_table(sqfs_file_t *file, sqfs_super_t *super,
			      sqfs_compressor_t *cmp,
			      const void *data, size_t table_size,
			      uint64_t *start);

SQFS_API int sqfs_read_table(sqfs_file_t *file, sqfs_compressor_t *cmp,
			     size_t table_size, uint64_t location,
			     uint64_t lower_limit, uint64_t upper_limit,
			     void **out);

#ifdef __cplusplus
}
#endif

#endif /* SQFS_TABLE_H */
