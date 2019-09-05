/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * super.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef SQFS_SUPER_H
#define SQFS_SUPER_H

#include "config.h"

#include "sqfs/predef.h"

#include <stdint.h>
#include <stddef.h>

#define SQFS_MAGIC 0x73717368
#define SQFS_VERSION_MAJOR 4
#define SQFS_VERSION_MINOR 0
#define SQFS_DEVBLK_SIZE 4096
#define SQFS_DEFAULT_BLOCK_SIZE 131072

typedef struct {
	uint32_t magic;
	uint32_t inode_count;
	uint32_t modification_time;
	uint32_t block_size;
	uint32_t fragment_entry_count;
	uint16_t compression_id;
	uint16_t block_log;
	uint16_t flags;
	uint16_t id_count;
	uint16_t version_major;
	uint16_t version_minor;
	uint64_t root_inode_ref;
	uint64_t bytes_used;
	uint64_t id_table_start;
	uint64_t xattr_id_table_start;
	uint64_t inode_table_start;
	uint64_t directory_table_start;
	uint64_t fragment_table_start;
	uint64_t export_table_start;
} __attribute__((packed)) sqfs_super_t;

typedef enum {
	SQFS_COMP_GZIP = 1,
	SQFS_COMP_LZMA = 2,
	SQFS_COMP_LZO = 3,
	SQFS_COMP_XZ = 4,
	SQFS_COMP_LZ4 = 5,
	SQFS_COMP_ZSTD = 6,

	SQFS_COMP_MIN = 1,
	SQFS_COMP_MAX = 6,
} E_SQFS_COMPRESSOR;

typedef enum {
	SQFS_FLAG_UNCOMPRESSED_INODES = 0x0001,
	SQFS_FLAG_UNCOMPRESSED_DATA = 0x0002,
	SQFS_FLAG_UNCOMPRESSED_FRAGMENTS = 0x0008,
	SQFS_FLAG_NO_FRAGMENTS = 0x0010,
	SQFS_FLAG_ALWAYS_FRAGMENTS = 0x0020,
	SQFS_FLAG_DUPLICATES = 0x0040,
	SQFS_FLAG_EXPORTABLE = 0x0080,
	SQFS_FLAG_UNCOMPRESSED_XATTRS = 0x0100,
	SQFS_FLAG_NO_XATTRS = 0x0200,
	SQFS_FLAG_COMPRESSOR_OPTIONS = 0x0400,
	SQFS_FLAG_UNCOMPRESSED_IDS = 0x0800,
} E_SQFS_SUPER_FLAGS;

#ifdef __cplusplus
extern "C" {
#endif

/* Returns 0 on success. Prints error messages to stderr on failure. */
SQFS_API int sqfs_super_init(sqfs_super_t *super, size_t block_size,
			     uint32_t mtime,
			     E_SQFS_COMPRESSOR compressor);

/* Returns 0 on success. Prints error messages to stderr on failure. */
SQFS_API int sqfs_super_write(sqfs_super_t *super, int fd);

/* Returns 0 on success. Prints error messages to stderr on failure. */
SQFS_API int sqfs_super_read(sqfs_super_t *super, int fd);

#ifdef __cplusplus
}
#endif

#endif /* SQFS_SUPER_H */
