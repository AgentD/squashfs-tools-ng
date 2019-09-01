/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * write_dir.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "sqfs/meta_writer.h"
#include "sqfs/inode.h"
#include "sqfs/dir.h"
#include "util.h"

#include <assert.h>
#include <endian.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static int get_type(mode_t mode)
{
	switch (mode & S_IFMT) {
	case S_IFSOCK: return SQFS_INODE_SOCKET;
	case S_IFIFO:  return SQFS_INODE_FIFO;
	case S_IFLNK:  return SQFS_INODE_SLINK;
	case S_IFBLK:  return SQFS_INODE_BDEV;
	case S_IFCHR:  return SQFS_INODE_CDEV;
	case S_IFDIR:  return SQFS_INODE_DIR;
	case S_IFREG:  return SQFS_INODE_FILE;
	default:
		assert(0);
	}
}

static int dir_index_grow(dir_index_t **index)
{
	size_t size = sizeof(dir_index_t) + sizeof(idx_ref_t) * 10;
	void *new;

	if (*index == NULL) {
		new = calloc(1, size);
	} else {
		if ((*index)->num_nodes < (*index)->max_nodes)
			return 0;

		size += sizeof(idx_ref_t) * (*index)->num_nodes;
		new = realloc(*index, size);
	}

	if (new == NULL) {
		perror("creating directory index");
		return -1;
	}

	*index = new;
	(*index)->max_nodes += 10;
	return 0;
}

int meta_writer_write_dir(meta_writer_t *dm, dir_info_t *dir,
			  dir_index_t **index)
{
	size_t i, size, count;
	sqfs_dir_header_t hdr;
	sqfs_dir_entry_t ent;
	tree_node_t *c, *d;
	uint16_t *diff_u16;
	uint32_t offset;
	uint64_t block;
	int32_t diff;

	c = dir->children;
	dir->size = 0;

	meta_writer_get_position(dm, &dir->start_block, &dir->block_offset);

	while (c != NULL) {
		meta_writer_get_position(dm, &block, &offset);

		count = 0;
		size = (offset + sizeof(hdr)) % SQFS_META_BLOCK_SIZE;

		for (d = c; d != NULL; d = d->next) {
			if ((d->inode_ref >> 16) != (c->inode_ref >> 16))
				break;

			diff = d->inode_num - c->inode_num;

			if (diff > 32767 || diff < -32767)
				break;

			size += sizeof(ent) + strlen(c->name);

			if (count > 0 && size > SQFS_META_BLOCK_SIZE)
				break;

			count += 1;
		}

		if (count > SQFS_MAX_DIR_ENT)
			count = SQFS_MAX_DIR_ENT;

		if (dir_index_grow(index))
			return -1;

		meta_writer_get_position(dm, &block, &offset);

		i = (*index)->num_nodes++;
		(*index)->idx_nodes[i].node = c;
		(*index)->idx_nodes[i].block = block;
		(*index)->idx_nodes[i].index = dir->size;

		hdr.count = htole32(count - 1);
		hdr.start_block = htole32(c->inode_ref >> 16);
		hdr.inode_number = htole32(c->inode_num);
		dir->size += sizeof(hdr);

		if (meta_writer_append(dm, &hdr, sizeof(hdr)))
			return -1;

		d = c;

		for (i = 0; i < count; ++i) {
			ent.inode_diff = c->inode_num - d->inode_num;

			diff_u16 = (uint16_t *)&ent.inode_diff;
			*diff_u16 = htole16(*diff_u16);

			ent.offset = htole16(c->inode_ref & 0x0000FFFF);
			ent.type = htole16(get_type(c->mode));
			ent.size = htole16(strlen(c->name) - 1);
			dir->size += sizeof(ent) + strlen(c->name);

			if (meta_writer_append(dm, &ent, sizeof(ent)))
				return -1;
			if (meta_writer_append(dm, c->name, strlen(c->name)))
				return -1;

			c = c->next;
		}
	}
	return 0;
}
