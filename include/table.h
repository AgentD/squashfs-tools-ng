/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TABLE_H
#define TABLE_H

#include "squashfs.h"
#include "compress.h"

#include <stdint.h>
#include <stddef.h>

/*
  Convenience function for writing meta data to a SquashFS image

  This function internally creates a meta data writer and writes 'count'
  blocks of data from 'data' to it, each 'entsize' bytes in size. For each
  meta data block, it remembers the 64 bit start address, writes out all
  addresses to an uncompressed address list and returns the location where
  the address list starts.

  Returns 0 on success. Internally prints error messages to stderr.
 */
int sqfs_write_table(int outfd, sqfs_super_t *super, const void *data,
		     size_t entsize, size_t count, uint64_t *startblock,
		     compressor_t *cmp);

#endif /* TABLE_H */
