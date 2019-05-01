/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef SQUASHFS_H
#define SQUASHFS_H

#include <stdint.h>
#include <stddef.h>

#define SQFS_MAGIC 0x73717368
#define SQFS_VERSION_MAJOR 4
#define SQFS_VERSION_MINOR 0
#define SQFS_META_BLOCK_SIZE 8192
#define SQFS_DEFAULT_BLOCK_SIZE 131072
#define SQFS_DEVBLK_SIZE 4096
#define SQFS_MAX_DIR_ENT 256

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

typedef struct {
	uint64_t start_offset;
	uint32_t size;
	uint32_t pad0;
} sqfs_fragment_t;

typedef struct {
	uint16_t type;
	uint16_t mode;
	uint16_t uid_idx;
	uint16_t gid_idx;
	uint32_t mod_time;
	uint32_t inode_number;
} sqfs_inode_t;

typedef struct {
	uint32_t nlink;
	uint32_t devno;
} sqfs_inode_dev_t;

typedef struct {
	uint32_t nlink;
} sqfs_inode_ipc_t;

typedef struct {
	uint32_t nlink;
	uint32_t devno;
	uint32_t xattr_idx;
} sqfs_inode_dev_ext_t;

typedef struct {
	uint32_t nlink;
	uint32_t target_size;
	uint8_t target[];
} sqfs_inode_slink_t;

typedef struct {
	uint32_t blocks_start;
	uint32_t fragment_index;
	uint32_t fragment_offset;
	uint32_t file_size;
	uint32_t block_sizes[];
} sqfs_inode_file_t;

typedef struct {
	uint64_t blocks_start;
	uint64_t file_size;
	uint64_t sparse;
	uint32_t nlink;
	uint32_t fragment_idx;
	uint32_t fragment_offset;
	uint32_t xattr_idx;
	uint32_t block_sizes[];
} sqfs_inode_file_ext_t;

typedef struct {
	uint32_t start_block;
	uint32_t nlink;
	uint16_t size;
	uint16_t offset;
	uint32_t parent_inode;
} sqfs_inode_dir_t;

typedef struct {
	uint32_t nlink;
	uint32_t size;
	uint32_t start_block;
	uint32_t parent_inode;
	uint16_t inodex_count;
	uint16_t offset;
	uint32_t xattr_idx;
} sqfs_inode_dir_ext_t;

typedef union {
	sqfs_inode_t inode;

	union {
		sqfs_inode_dev_t dev;
		sqfs_inode_dev_ext_t dev_ext;
		sqfs_inode_slink_t slink;
		sqfs_inode_file_t file;
		sqfs_inode_file_ext_t file_ext;
		sqfs_inode_dir_t dir;
		sqfs_inode_dir_ext_t dir_ext;
	} data;
} sqfs_inode_generic_t;

typedef struct {
	uint32_t count;
	uint32_t start_block;
	uint32_t inode_number;
} sqfs_dir_header_t;

typedef struct {
	uint16_t offset;
	uint16_t inode_number;
	uint16_t type;
	uint16_t size;
	uint8_t name[];
} sqfs_dir_entry_t;

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

typedef enum {
	SQFS_INODE_DIR = 1,
	SQFS_INODE_FILE = 2,
	SQFS_INODE_SLINK = 3,
	SQFS_INODE_BDEV = 4,
	SQFS_INODE_CDEV = 5,
	SQFS_INODE_FIFO = 6,
	SQFS_INODE_SOCKET = 7,
	SQFS_INODE_EXT_DIR = 8,
	SQFS_INODE_EXT_FILE = 9,
	SQFS_INODE_EXT_SLINK = 10,
	SQFS_INODE_EXT_BDEV = 11,
	SQFS_INODE_EXT_CDEV = 12,
	SQFS_INODE_EXT_FIFO = 13,
	SQFS_INODE_EXT_SOCKET = 14,
} E_SQFS_INODE_TYPE;

int sqfs_super_init(sqfs_super_t *super, size_t block_size, uint32_t mtime,
		    E_SQFS_COMPRESSOR compressor);

int sqfs_super_write(sqfs_super_t *super, int fd);

#endif /* SQUASHFS_H */
