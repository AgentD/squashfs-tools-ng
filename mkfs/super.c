/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "mksquashfs.h"
#include "util.h"

#include <endian.h>

int sqfs_super_init(sqfs_info_t *info)
{
	unsigned int i;

	if (info->opt.blksz & (info->opt.blksz - 1)) {
		fputs("Block size must be a power of 2!\n", stderr);
		return -1;
	}

	if (info->opt.blksz < 8192 || info->opt.blksz >= (1 << 24)) {
		fputs("Block size must be between 8k and 1M\n", stderr);
		return -1;
	}

	memset(&info->super, 0, sizeof(info->super));
	info->super.magic = SQFS_MAGIC;
	info->super.modification_time = info->opt.def_mtime;
	info->super.block_size = info->opt.blksz;
	info->super.compression_id = info->opt.compressor;
	info->super.flags = SQFS_FLAG_NO_FRAGMENTS | SQFS_FLAG_NO_XATTRS;
	info->super.version_major = SQFS_VERSION_MAJOR;
	info->super.version_minor = SQFS_VERSION_MINOR;
	info->super.bytes_used = sizeof(info->super);
	info->super.id_table_start = 0xFFFFFFFFFFFFFFFFUL;
	info->super.xattr_id_table_start = 0xFFFFFFFFFFFFFFFFUL;
	info->super.inode_table_start = 0xFFFFFFFFFFFFFFFFUL;
	info->super.directory_table_start = 0xFFFFFFFFFFFFFFFFUL;
	info->super.fragment_table_start = 0xFFFFFFFFFFFFFFFFUL;
	info->super.export_table_start = 0xFFFFFFFFFFFFFFFFUL;

	for (i = info->opt.blksz; i != 0x01; i >>= 1)
		info->super.block_log += 1;

	return 0;
}

int sqfs_padd_file(sqfs_info_t *info)
{
	size_t padd_sz = info->super.bytes_used % info->opt.devblksz;
	uint8_t *buffer;
	ssize_t ret;

	if (padd_sz == 0)
		return 0;

	padd_sz = info->opt.devblksz - padd_sz;

	buffer = calloc(1, padd_sz);
	if (buffer == NULL) {
		perror("padding output file to block size");
		return -1;
	}

	ret = write_retry(info->outfd, buffer, padd_sz);

	if (ret < 0) {
		perror("Error padding squashfs image to page size");
		free(buffer);
		return -1;
	}

	if ((size_t)ret < padd_sz) {
		fputs("Truncated write trying to padd squashfs image\n",
		      stderr);
		return -1;
	}

	free(buffer);
	return 0;
}

int sqfs_super_write(sqfs_info_t *info)
{
	sqfs_super_t copy;
	ssize_t ret;

	copy.magic = htole32(info->super.magic);
	copy.inode_count = htole32(info->super.inode_count);
	copy.modification_time = htole32(info->super.modification_time);
	copy.block_size = htole32(info->super.block_size);
	copy.fragment_entry_count = htole32(info->super.fragment_entry_count);
	copy.compression_id = htole16(info->super.compression_id);
	copy.block_log = htole16(info->super.block_log);
	copy.flags = htole16(info->super.flags);
	copy.id_count = htole16(info->super.id_count);
	copy.version_major = htole16(info->super.version_major);
	copy.version_minor = htole16(info->super.version_minor);
	copy.root_inode_ref = htole64(info->super.root_inode_ref);
	copy.bytes_used = htole64(info->super.bytes_used);
	copy.id_table_start = htole64(info->super.id_table_start);
	copy.xattr_id_table_start = htole64(info->super.xattr_id_table_start);
	copy.inode_table_start = htole64(info->super.inode_table_start);
	copy.directory_table_start = htole64(info->super.directory_table_start);
	copy.fragment_table_start = htole64(info->super.fragment_table_start);
	copy.export_table_start = htole64(info->super.export_table_start);

	if (lseek(info->outfd, 0, SEEK_SET) == (off_t)-1)
		goto fail_seek;

	ret = write_retry(info->outfd, &copy, sizeof(copy));

	if (ret < 0) {
		perror("squashfs writing super block");
		return -1;
	}

	if ((size_t)ret < sizeof(copy)) {
		fputs("squashfs writing super block: truncated write\n",
		      stderr);
		return -1;
	}

	if (lseek(info->outfd, 0, SEEK_END) == (off_t)-1)
		goto fail_seek;

	return 0;
fail_seek:
	perror("squashfs writing super block: seek on output file");
	return -1;
}
