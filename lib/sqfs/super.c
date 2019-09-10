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
#include "sqfs/io.h"
#include "util.h"

#include <endian.h>
#include <string.h>
#include <unistd.h>

int sqfs_super_init(sqfs_super_t *super, size_t block_size, uint32_t mtime,
		    E_SQFS_COMPRESSOR compressor)
{
	unsigned int i;

	if (block_size & (block_size - 1))
		return SQFS_ERROR_SUPER_BLOCK_SIZE;

	if (block_size < 4096 || block_size >= (1 << 20))
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

int sqfs_super_write(const sqfs_super_t *super, sqfs_file_t *file)
{
	sqfs_super_t copy;

	copy.magic = htole32(super->magic);
	copy.inode_count = htole32(super->inode_count);
	copy.modification_time = htole32(super->modification_time);
	copy.block_size = htole32(super->block_size);
	copy.fragment_entry_count = htole32(super->fragment_entry_count);
	copy.compression_id = htole16(super->compression_id);
	copy.block_log = htole16(super->block_log);
	copy.flags = htole16(super->flags);
	copy.id_count = htole16(super->id_count);
	copy.version_major = htole16(super->version_major);
	copy.version_minor = htole16(super->version_minor);
	copy.root_inode_ref = htole64(super->root_inode_ref);
	copy.bytes_used = htole64(super->bytes_used);
	copy.id_table_start = htole64(super->id_table_start);
	copy.xattr_id_table_start = htole64(super->xattr_id_table_start);
	copy.inode_table_start = htole64(super->inode_table_start);
	copy.directory_table_start = htole64(super->directory_table_start);
	copy.fragment_table_start = htole64(super->fragment_table_start);
	copy.export_table_start = htole64(super->export_table_start);

	return file->write_at(file, 0, &copy, sizeof(copy));
}
