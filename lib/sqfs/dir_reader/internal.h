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

	sqfs_readdir_state_t it;

	sqfs_u32 flags;

	int start_state;
	int state;
	sqfs_u64 parent_ref;
	sqfs_u64 cur_ref;
	sqfs_u64 ent_ref;
	rbtree_t dcache;
};

#endif /* DIR_READER_INTERNAL_H */
