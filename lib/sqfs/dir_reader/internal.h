/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * internal.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef DIR_READER_INTERNAL_H
#define DIR_READER_INTERNAL_H

#include "config.h"

#include "sqfs/meta_reader.h"
#include "sqfs/dir_reader.h"
#include "sqfs/compressor.h"
#include "sqfs/id_table.h"
#include "sqfs/super.h"
#include "sqfs/inode.h"
#include "sqfs/error.h"
#include "sqfs/dir.h"
#include "rbtree.h"
#include "util.h"

#include <string.h>
#include <stdlib.h>

enum {
	DIR_STATE_NONE = 0,
	DIR_STATE_OPENED = 1,
	DIR_STATE_DOT = 2,
	DIR_STATE_DOT_DOT = 3,
	DIR_STATE_ENTRIES = 4,
};

struct sqfs_dir_reader_t {
	sqfs_object_t base;

	sqfs_meta_reader_t *meta_dir;
	sqfs_meta_reader_t *meta_inode;
	const sqfs_super_t *super;

	sqfs_dir_header_t hdr;
	sqfs_u64 dir_block_start;
	size_t entries;
	size_t size;

	size_t start_size;
	sqfs_u16 dir_offset;
	sqfs_u16 inode_offset;

	sqfs_u32 flags;

	int start_state;
	int state;
	sqfs_u64 parent_ref;
	sqfs_u64 cur_ref;
	rbtree_t dcache;
};

SQFS_INTERNAL int sqfs_dir_reader_dcache_init(sqfs_dir_reader_t *rd,
					      sqfs_u32 flags);

SQFS_INTERNAL int sqfs_dir_reader_dcache_init_copy(sqfs_dir_reader_t *copy,
						   const sqfs_dir_reader_t *rd);

SQFS_INTERNAL int sqfs_dir_reader_dcache_add(sqfs_dir_reader_t *rd,
					     sqfs_u32 inode, sqfs_u64 ref);

SQFS_INTERNAL int sqfs_dir_reader_dcache_find(sqfs_dir_reader_t *rd,
					      sqfs_u32 inode, sqfs_u64 *ref);

SQFS_INTERNAL void sqfs_dir_reader_dcache_cleanup(sqfs_dir_reader_t *rd);

#endif /* DIR_READER_INTERNAL_H */
