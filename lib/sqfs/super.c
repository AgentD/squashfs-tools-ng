/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * super.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "config.h"

#include "sqfs/super.h"
#include "sqfs/error.h"

#include <string.h>

int sqfs_super_init(sqfs_super_t *super, size_t block_size, sqfs_u32 mtime,
		    E_SQFS_COMPRESSOR compressor)
{
	unsigned int i;

	if (block_size & (block_size - 1))
		return SQFS_ERROR_SUPER_BLOCK_SIZE;

	if (block_size < 4096 || block_size > (1 << 20))
		return SQFS_ERROR_SUPER_BLOCK_SIZE;

	memset(super, 0, sizeof(*super));
	super->magic = SQFS_MAGIC;
	super->modification_time = mtime;
	super->block_size = block_size;
	super->compression_id = compressor;
	super->flags = SQFS_FLAG_NO_FRAGMENTS | SQFS_FLAG_NO_XATTRS;
	super->version_major = SQFS_VERSION_MAJOR;
	super->version_minor = SQFS_VERSION_MINOR;
	super->bytes_used = sizeof(*super);
	super->id_table_start = 0xFFFFFFFFFFFFFFFFUL;
	super->xattr_id_table_start = 0xFFFFFFFFFFFFFFFFUL;
	super->inode_table_start = 0xFFFFFFFFFFFFFFFFUL;
	super->directory_table_start = 0xFFFFFFFFFFFFFFFFUL;
	super->fragment_table_start = 0xFFFFFFFFFFFFFFFFUL;
	super->export_table_start = 0xFFFFFFFFFFFFFFFFUL;

	for (i = block_size; i != 0x01; i >>= 1)
		super->block_log += 1;

	return 0;
}
