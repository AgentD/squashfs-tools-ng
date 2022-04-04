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
#include "util.h"

#include <string.h>
#include <stdlib.h>

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
};

#endif /* DIR_READER_INTERNAL_H */
