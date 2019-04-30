/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "mksquashfs.h"
#include "util.h"

#include <endian.h>

int sqfs_super_init(sqfs_super_t *s, const options_t *opt)
{
	unsigned int i;

	if (opt->blksz & (opt->blksz - 1)) {
		fputs("Block size must be a power of 2!\n", stderr);
		return -1;
	}

	if (opt->blksz < 8192 || opt->blksz >= (1 << 24)) {
		fputs("Block size must be between 8k and 1M\n", stderr);
		return -1;
	}

	memset(s, 0, sizeof(*s));
	s->magic = SQFS_MAGIC;
	s->modification_time = opt->def_mtime;
	s->block_size = opt->blksz;
	s->compression_id = opt->compressor;
	s->flags = SQFS_FLAG_NO_FRAGMENTS | SQFS_FLAG_NO_XATTRS;
	s->version_major = SQFS_VERSION_MAJOR;
	s->version_minor = SQFS_VERSION_MINOR;
	s->bytes_used = sizeof(*s);
	s->id_table_start = 0xFFFFFFFFFFFFFFFFUL;
	s->xattr_id_table_start = 0xFFFFFFFFFFFFFFFFUL;
	s->inode_table_start = 0xFFFFFFFFFFFFFFFFUL;
	s->directory_table_start = 0xFFFFFFFFFFFFFFFFUL;
	s->fragment_table_start = 0xFFFFFFFFFFFFFFFFUL;
	s->export_table_start = 0xFFFFFFFFFFFFFFFFUL;

	for (i = opt->blksz; i != 0x01; i >>= 1)
		s->block_log += 1;

	return 0;
}

int sqfs_padd_file(sqfs_super_t *s, const options_t *opt, int outfd)
{
	size_t padd_sz = s->bytes_used % opt->devblksz;
	uint8_t *buffer;
	ssize_t ret;

	if (padd_sz == 0)
		return 0;

	padd_sz = opt->devblksz - padd_sz;

	buffer = calloc(1, padd_sz);
	if (buffer == NULL) {
		perror("padding output file to block size");
		return -1;
	}

	ret = write_retry(outfd, buffer, padd_sz);

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

int sqfs_super_write(const sqfs_super_t *super, int outfd)
{
	sqfs_super_t copy;
	ssize_t ret;

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

	if (lseek(outfd, 0, SEEK_SET) == (off_t)-1)
		goto fail_seek;

	ret = write_retry(outfd, &copy, sizeof(copy));

	if (ret < 0) {
		perror("squashfs writing super block");
		return -1;
	}

	if ((size_t)ret < sizeof(copy)) {
		fputs("squashfs writing super block: truncated write\n",
		      stderr);
		return -1;
	}

	if (lseek(outfd, 0, SEEK_END) == (off_t)-1)
		goto fail_seek;

	return 0;
fail_seek:
	perror("squashfs writing super block: seek on output file");
	return -1;
}
