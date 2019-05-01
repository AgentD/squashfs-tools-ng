/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TABLE_H
#define TABLE_H

#include "squashfs.h"
#include "compress.h"

#include <stdint.h>
#include <stddef.h>

int sqfs_write_table(int outfd, sqfs_super_t *super, const void *data,
		     size_t entsize, size_t count, uint64_t *startblock,
		     compressor_t *cmp);

#endif /* TABLE_H */
