/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "squashfs.h"
#include "util.h"

#include <endian.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

int sqfs_super_read(sqfs_super_t *super, int fd)
{
	size_t block_size = 0;
	sqfs_super_t temp;
	ssize_t ret;
	int i;

	if (lseek(fd, 0, SEEK_SET) == (off_t)-1)
		goto fail_seek;

	ret = read_retry(fd, &temp, sizeof(temp));

	if (ret < 0) {
		perror("reading super block");
		return -1;
	}

	if ((size_t)ret < sizeof(temp)) {
		fputs("reading super block: unexpected end of file\n", stderr);
		return -1;
	}

	temp.magic = le32toh(temp.magic);
	temp.inode_count = le32toh(temp.inode_count);
	temp.modification_time = le32toh(temp.modification_time);
	temp.block_size = le32toh(temp.block_size);
	temp.fragment_entry_count = le32toh(temp.fragment_entry_count);
	temp.compression_id = le16toh(temp.compression_id);
	temp.block_log = le16toh(temp.block_log);
	temp.flags = le16toh(temp.flags);
	temp.id_count = le16toh(temp.id_count);
	temp.version_major = le16toh(temp.version_major);
	temp.version_minor = le16toh(temp.version_minor);
	temp.root_inode_ref = le64toh(temp.root_inode_ref);
	temp.bytes_used = le64toh(temp.bytes_used);
	temp.id_table_start = le64toh(temp.id_table_start);
	temp.xattr_id_table_start = le64toh(temp.xattr_id_table_start);
	temp.inode_table_start = le64toh(temp.inode_table_start);
	temp.directory_table_start = le64toh(temp.directory_table_start);
	temp.fragment_table_start = le64toh(temp.fragment_table_start);
	temp.export_table_start = le64toh(temp.export_table_start);

	if (temp.magic != SQFS_MAGIC) {
		fputs("Magic number missing. Not a squashfs image.\n", stderr);
		return -1;
	}

	if ((temp.version_major != SQFS_VERSION_MAJOR) ||
	    (temp.version_minor != SQFS_VERSION_MINOR)) {
		fprintf(stderr,
			"The squashfs image uses squashfs version %d.%d\n"
			"This tool currently only supports version %d.%d.\n",
			temp.version_major, temp.version_minor,
			SQFS_VERSION_MAJOR, SQFS_VERSION_MINOR);
		return -1;
	}

	if ((temp.block_size - 1) & temp.block_size) {
		fputs("Block size in image is not a power of 2!\n", stderr);
		return -1;
	}

	if (temp.block_log > 0 && temp.block_log < 32) {
		block_size = 1;

		for (i = 0; i < temp.block_log; ++i)
			block_size <<= 1;
	}

	if (temp.block_size != block_size) {
		fputs("Mismatch between block size and block log\n", stderr);
		fputs("Filesystem probably currupted.\n", stderr);
		return -1;
	}

	if (temp.compression_id < SQFS_COMP_MIN ||
	    temp.compression_id > SQFS_COMP_MAX) {
		fputs("Image uses an unsupported compressor\n", stderr);
		return -1;
	}

	if (temp.id_count == 0) {
		fputs("ID table in squashfs image is empty.\n", stderr);
		return -1;
	}

	memcpy(super, &temp, sizeof(temp));
	return 0;
fail_seek:
	perror("squashfs writing super block: seek on output file");
	return -1;
}
