/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * read_inode.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "sqfs/inode.h"
#include "util.h"

#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#define SWAB16(x) x = le16toh(x)
#define SWAB32(x) x = le32toh(x)
#define SWAB64(x) x = le64toh(x)

static int set_mode(sqfs_inode_t *inode)
{
	inode->mode &= ~S_IFMT;

	switch (inode->type) {
	case SQFS_INODE_SOCKET:
	case SQFS_INODE_EXT_SOCKET:
		inode->mode |= S_IFSOCK;
		break;
	case SQFS_INODE_SLINK:
	case SQFS_INODE_EXT_SLINK:
		inode->mode |= S_IFLNK;
		break;
	case SQFS_INODE_FILE:
	case SQFS_INODE_EXT_FILE:
		inode->mode |= S_IFREG;
		break;
	case SQFS_INODE_BDEV:
	case SQFS_INODE_EXT_BDEV:
		inode->mode |= S_IFBLK;
		break;
	case SQFS_INODE_DIR:
	case SQFS_INODE_EXT_DIR:
		inode->mode |= S_IFDIR;
		break;
	case SQFS_INODE_CDEV:
	case SQFS_INODE_EXT_CDEV:
		inode->mode |= S_IFCHR;
		break;
	case SQFS_INODE_FIFO:
	case SQFS_INODE_EXT_FIFO:
		inode->mode |= S_IFIFO;
		break;
	default:
		fputs("Found inode with unknown file mode\n", stderr);
		return -1;
	}

	return 0;
}

static uint64_t get_block_count(uint64_t size, uint64_t block_size,
				uint32_t frag_index, uint32_t frag_offset)
{
	uint64_t count = size / block_size;

	if ((size % block_size) != 0 &&
	    (frag_index == 0xFFFFFFFF || frag_offset == 0xFFFFFFFF)) {
		++count;
	}

	return count;
}

static sqfs_inode_generic_t *read_inode_file(sqfs_meta_reader_t *ir,
					     sqfs_inode_t *base,
					     size_t block_size)
{
	sqfs_inode_generic_t *out;
	sqfs_inode_file_t file;
	uint64_t i, count;

	if (sqfs_meta_reader_read(ir, &file, sizeof(file)))
		return NULL;

	SWAB32(file.blocks_start);
	SWAB32(file.fragment_index);
	SWAB32(file.fragment_offset);
	SWAB32(file.file_size);

	count = get_block_count(file.file_size, block_size,
				file.fragment_index, file.fragment_offset);

	out = alloc_flex(sizeof(*out), sizeof(uint32_t), count);
	if (out == NULL) {
		perror("reading extended file inode");
		return NULL;
	}

	out->base = *base;
	out->data.file = file;
	out->block_sizes = (uint32_t *)out->extra;
	out->num_file_blocks = count;

	if (sqfs_meta_reader_read(ir, out->block_sizes,
				  count * sizeof(uint32_t))) {
		free(out);
		return NULL;
	}

	for (i = 0; i < count; ++i)
		SWAB32(out->block_sizes[i]);

	return out;
}

static sqfs_inode_generic_t *read_inode_file_ext(sqfs_meta_reader_t *ir,
						 sqfs_inode_t *base,
						 size_t block_size)
{
	sqfs_inode_file_ext_t file;
	sqfs_inode_generic_t *out;
	uint64_t i, count;

	if (sqfs_meta_reader_read(ir, &file, sizeof(file)))
		return NULL;

	SWAB64(file.blocks_start);
	SWAB64(file.file_size);
	SWAB64(file.sparse);
	SWAB32(file.nlink);
	SWAB32(file.fragment_idx);
	SWAB32(file.fragment_offset);
	SWAB32(file.xattr_idx);

	count = get_block_count(file.file_size, block_size,
				file.fragment_idx, file.fragment_offset);

	out = alloc_flex(sizeof(*out), sizeof(uint32_t), count);
	if (out == NULL) {
		perror("reading extended file inode");
		return NULL;
	}

	out->base = *base;
	out->data.file_ext = file;
	out->block_sizes = (uint32_t *)out->extra;
	out->num_file_blocks = count;

	if (sqfs_meta_reader_read(ir, out->block_sizes,
				  count * sizeof(uint32_t))) {
		free(out);
		return NULL;
	}

	for (i = 0; i < count; ++i)
		SWAB32(out->block_sizes[i]);

	return out;
}

static sqfs_inode_generic_t *read_inode_slink(sqfs_meta_reader_t *ir,
					      sqfs_inode_t *base)
{
	sqfs_inode_generic_t *out;
	sqfs_inode_slink_t slink;
	size_t size;

	if (sqfs_meta_reader_read(ir, &slink, sizeof(slink)))
		return NULL;

	SWAB32(slink.nlink);
	SWAB32(slink.target_size);

	if (SZ_ADD_OV(slink.target_size, 1, &size) ||
	    SZ_ADD_OV(sizeof(*out), size, &size)) {
		errno = EOVERFLOW;
		goto fail;
	}

	out = calloc(1, size);
	if (out == NULL)
		goto fail;

	out->slink_target = (char *)out->extra;
	out->base = *base;
	out->data.slink = slink;

	if (sqfs_meta_reader_read(ir, out->slink_target, slink.target_size)) {
		free(out);
		return NULL;
	}

	return out;
fail:
	perror("reading symlink inode");
	return NULL;
}

static sqfs_inode_generic_t *read_inode_slink_ext(sqfs_meta_reader_t *ir,
						  sqfs_inode_t *base)
{
	sqfs_inode_generic_t *out = read_inode_slink(ir, base);
	uint32_t xattr;

	if (out != NULL) {
		if (sqfs_meta_reader_read(ir, &xattr, sizeof(xattr))) {
			free(out);
			return NULL;
		}

		out->data.slink_ext.xattr_idx = le32toh(xattr);
	}

	return out;
}

sqfs_inode_generic_t *sqfs_meta_reader_read_inode(sqfs_meta_reader_t *ir,
						  sqfs_super_t *super,
						  uint64_t block_start,
						  size_t offset)
{
	sqfs_inode_generic_t *out;
	sqfs_inode_t inode;

	/* read base inode */
	block_start += super->inode_table_start;

	if (sqfs_meta_reader_seek(ir, block_start, offset))
		return NULL;

	if (sqfs_meta_reader_read(ir, &inode, sizeof(inode)))
		return NULL;

	SWAB16(inode.type);
	SWAB16(inode.mode);
	SWAB16(inode.uid_idx);
	SWAB16(inode.gid_idx);
	SWAB32(inode.mod_time);
	SWAB32(inode.inode_number);

	if (set_mode(&inode))
		return NULL;

	/* inode types where the size is variable */
	switch (inode.type) {
	case SQFS_INODE_FILE:
		return read_inode_file(ir, &inode, super->block_size);
	case SQFS_INODE_SLINK:
		return read_inode_slink(ir, &inode);
	case SQFS_INODE_EXT_FILE:
		return read_inode_file_ext(ir, &inode, super->block_size);
	case SQFS_INODE_EXT_SLINK:
		return read_inode_slink_ext(ir, &inode);
	default:
		break;
	}

	/* everything else */
	out = calloc(1, sizeof(*out));
	if (out == NULL) {
		perror("reading symlink inode");
		return NULL;
	}

	out->base = inode;

	switch (inode.type) {
	case SQFS_INODE_DIR:
		if (sqfs_meta_reader_read(ir, &out->data.dir,
					  sizeof(out->data.dir))) {
			goto fail_free;
		}
		SWAB32(out->data.dir.start_block);
		SWAB32(out->data.dir.nlink);
		SWAB16(out->data.dir.size);
		SWAB16(out->data.dir.offset);
		SWAB32(out->data.dir.parent_inode);
		break;
	case SQFS_INODE_BDEV:
	case SQFS_INODE_CDEV:
		if (sqfs_meta_reader_read(ir, &out->data.dev,
					  sizeof(out->data.dev))) {
			goto fail_free;
		}
		SWAB32(out->data.dev.nlink);
		SWAB32(out->data.dev.devno);
		break;
	case SQFS_INODE_FIFO:
	case SQFS_INODE_SOCKET:
		if (sqfs_meta_reader_read(ir, &out->data.ipc,
					  sizeof(out->data.ipc))) {
			goto fail_free;
		}
		SWAB32(out->data.ipc.nlink);
		break;
	case SQFS_INODE_EXT_DIR:
		if (sqfs_meta_reader_read(ir, &out->data.dir_ext,
					  sizeof(out->data.dir_ext))) {
			goto fail_free;
		}
		SWAB32(out->data.dir_ext.nlink);
		SWAB32(out->data.dir_ext.size);
		SWAB32(out->data.dir_ext.start_block);
		SWAB32(out->data.dir_ext.parent_inode);
		SWAB16(out->data.dir_ext.inodex_count);
		SWAB16(out->data.dir_ext.offset);
		SWAB32(out->data.dir_ext.xattr_idx);
		break;
	case SQFS_INODE_EXT_BDEV:
	case SQFS_INODE_EXT_CDEV:
		if (sqfs_meta_reader_read(ir, &out->data.dev_ext,
					  sizeof(out->data.dev_ext))) {
			goto fail_free;
		}
		SWAB32(out->data.dev_ext.nlink);
		SWAB32(out->data.dev_ext.devno);
		SWAB32(out->data.dev_ext.xattr_idx);
		break;
	case SQFS_INODE_EXT_FIFO:
	case SQFS_INODE_EXT_SOCKET:
		if (sqfs_meta_reader_read(ir, &out->data.ipc_ext,
					  sizeof(out->data.ipc_ext))) {
			goto fail_free;
		}
		SWAB32(out->data.ipc_ext.nlink);
		SWAB32(out->data.ipc_ext.xattr_idx);
		break;
	default:
		fputs("Unknown inode type found\n", stderr);
		goto fail_free;
	}

	return out;
fail_free:
	free(out);
	return NULL;
}
