/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TABLE_H
#define TABLE_H

#include "squashfs.h"
#include "compress.h"

#include <stdint.h>
#include <stddef.h>

/**
 * @brief Convenience function for writing meta data to a SquashFS image
 *
 * @note This function internally prints error message to stderr on failure
 *
 * This function internally creates a meta data writer and writes 'count'
 * blocks of data from 'data' to it, each 'entsize' bytes in size. For each
 * meta data block, it remembers the 64 bit start address, writes out all
 * addresses to the and returns the location where the address list starts.
 * For instance, the fragment table and ID table are stored in this format.
 *
 * @param outfd      The file descriptor to write to
 * @param super      A pointer to the SquashFS super block
 * @param data       A pointer to the data to write out
 * @param entsize    The size of each data record
 * @param count      The number of data records to write out
 * @param startblock Returns the location of the lookup table
 * @param cmp        A pointer to the compressor to use for the meta data writer
 *
 * @return Zero on success, -1 on failure
 */
int sqfs_write_table(int outfd, sqfs_super_t *super, const void *data,
		     size_t entsize, size_t count, uint64_t *startblock,
		     compressor_t *cmp);

#endif /* TABLE_H */
