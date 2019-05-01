/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TABLE_H
#define TABLE_H

#include "squashfs.h"
#include "compress.h"

#include <stdint.h>
#include <stddef.h>

int sqfs_write_fragment_table(int outfd, sqfs_super_t *super,
			      sqfs_fragment_t *fragments, size_t count,
			      compressor_t *cmp);

int sqfs_write_ids(int outfd, sqfs_super_t *super, uint32_t *id_tbl,
		   size_t count, compressor_t *cmp);

#endif /* TABLE_H */
