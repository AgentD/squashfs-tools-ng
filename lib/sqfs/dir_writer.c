/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * dir_writer.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "config.h"

#include "sqfs/meta_writer.h"
#include "sqfs/dir_writer.h"
#include "sqfs/inode.h"
#include "sqfs/error.h"
#include "sqfs/block.h"
#include "sqfs/dir.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>

typedef struct dir_entry_t {
	struct dir_entry_t *next;
	sqfs_u64 inode_ref;
	sqfs_u32 inode_num;
	sqfs_u16 type;
	size_t name_len;
	char name[];
} dir_entry_t;

typedef struct index_ent_t {
	struct index_ent_t *next;
	dir_entry_t *ent;
	sqfs_u64 block;
	sqfs_u32 index;
} index_ent_t;

struct sqfs_dir_writer_t {
	dir_entry_t *list;
	dir_entry_t *list_end;

	index_ent_t *idx;
	index_ent_t *idx_end;

	sqfs_u64 dir_ref;
	size_t dir_size;
	size_t idx_size;
	size_t ent_count;
	sqfs_meta_writer_t *dm;
};

static int get_type(sqfs_u16 mode)
{
	switch (mode & S_IFMT) {
	case S_IFSOCK: return SQFS_INODE_SOCKET;
	case S_IFIFO:  return SQFS_INODE_FIFO;
	case S_IFLNK:  return SQFS_INODE_SLINK;
	case S_IFBLK:  return SQFS_INODE_BDEV;
	case S_IFCHR:  return SQFS_INODE_CDEV;
	case S_IFDIR:  return SQFS_INODE_DIR;
	case S_IFREG:  return SQFS_INODE_FILE;
	}

	return SQFS_ERROR_UNSUPPORTED;
}

static void writer_reset(sqfs_dir_writer_t *writer)
{
	dir_entry_t *ent;
	index_ent_t *idx;

	while (writer->idx != NULL) {
		idx = writer->idx;
		writer->idx = idx->next;
		free(idx);
	}

	while (writer->list != NULL) {
		ent = writer->list;
		writer->list = ent->next;
		free(ent);
	}

	writer->list_end = NULL;
	writer->idx_end = NULL;
	writer->dir_ref = 0;
	writer->dir_size = 0;
	writer->idx_size = 0;
	writer->ent_count = 0;
}

sqfs_dir_writer_t *sqfs_dir_writer_create(sqfs_meta_writer_t *dm)
{
	sqfs_dir_writer_t *writer = calloc(1, sizeof(*writer));

	if (writer == NULL)
		return NULL;

	writer->dm = dm;
	return writer;
}

void sqfs_dir_writer_destroy(sqfs_dir_writer_t *writer)
{
	writer_reset(writer);
	free(writer);
}

int sqfs_dir_writer_begin(sqfs_dir_writer_t *writer, sqfs_u16 flags)
{
	sqfs_u32 offset;
	sqfs_u64 block;

	if (flags != 0)
		return SQFS_ERROR_UNSUPPORTED;

	writer_reset(writer);

	sqfs_meta_writer_get_position(writer->dm, &block, &offset);
	writer->dir_ref = (block << 16) | offset;
	return 0;
}

int sqfs_dir_writer_add_entry(sqfs_dir_writer_t *writer, const char *name,
			      sqfs_u32 inode_num, sqfs_u64 inode_ref,
			      sqfs_u16 mode)
{
	dir_entry_t *ent;
	int type;

	type = get_type(mode);
	if (type < 0)
		return type;

	ent = alloc_flex(sizeof(*ent), 1, strlen(name));
	if (ent == NULL)
		return SQFS_ERROR_ALLOC;

	ent->inode_ref = inode_ref;
	ent->inode_num = inode_num;
	ent->type = type;
	ent->name_len = strlen(name);
	memcpy(ent->name, name, ent->name_len);

	if (writer->list_end == NULL) {
		writer->list = writer->list_end = ent;
	} else {
		writer->list_end->next = ent;
		writer->list_end = ent;
	}

	writer->ent_count += 1;
	return 0;
}

static size_t get_conseq_entry_count(sqfs_u32 offset, dir_entry_t *head)
{
	size_t size, count = 0;
	dir_entry_t *it;
	sqfs_s32 diff;

	size = (offset + sizeof(sqfs_dir_header_t)) % SQFS_META_BLOCK_SIZE;

	for (it = head; it != NULL; it = it->next) {
		if ((it->inode_ref >> 16) != (head->inode_ref >> 16))
			break;

		diff = it->inode_num - head->inode_num;

		if (diff > 32767 || diff < -32767)
			break;

		size += sizeof(sqfs_dir_entry_t) + it->name_len;

		if (count > 0 && size > SQFS_META_BLOCK_SIZE)
			break;

		count += 1;

		if (count == SQFS_MAX_DIR_ENT)
			break;
	}

	return count;
}

static int add_header(sqfs_dir_writer_t *writer, size_t count,
		      dir_entry_t *ref, sqfs_u64 block)
{
	sqfs_dir_header_t hdr;
	index_ent_t *idx;
	int err;

	hdr.count = htole32(count - 1);
	hdr.start_block = htole32(ref->inode_ref >> 16);
	hdr.inode_number = htole32(ref->inode_num);

	err = sqfs_meta_writer_append(writer->dm, &hdr, sizeof(hdr));
	if (err)
		return err;

	idx = calloc(1, sizeof(*idx));
	if (idx == NULL)
		return SQFS_ERROR_ALLOC;

	idx->ent = ref;
	idx->block = block;
	idx->index = writer->dir_size;

	if (writer->idx_end == NULL) {
		writer->idx = writer->idx_end = idx;
	} else {
		writer->idx_end->next = idx;
		writer->idx_end = idx;
	}

	writer->dir_size += sizeof(hdr);
	writer->idx_size += 1;
	return 0;
}

int sqfs_dir_writer_end(sqfs_dir_writer_t *writer)
{
	dir_entry_t *it, *first;
	sqfs_dir_entry_t ent;
	sqfs_u16 *diff_u16;
	size_t i, count;
	sqfs_u32 offset;
	sqfs_u64 block;
	int err;

	for (it = writer->list; it != NULL; ) {
		sqfs_meta_writer_get_position(writer->dm, &block, &offset);
		count = get_conseq_entry_count(offset, it);

		err = add_header(writer, count, it, block);
		if (err)
			return err;

		first = it;

		for (i = 0; i < count; ++i) {
			ent.offset = htole16(it->inode_ref & 0x0000FFFF);
			ent.inode_diff = it->inode_num - first->inode_num;
			ent.type = htole16(it->type);
			ent.size = htole16(it->name_len - 1);

			diff_u16 = (sqfs_u16 *)&ent.inode_diff;
			*diff_u16 = htole16(*diff_u16);

			err = sqfs_meta_writer_append(writer->dm, &ent,
						      sizeof(ent));
			if (err)
				return err;

			err = sqfs_meta_writer_append(writer->dm, it->name,
						      it->name_len);
			if (err)
				return err;

			writer->dir_size += sizeof(ent) + it->name_len;
			it = it->next;
		}
	}

	return 0;
}

size_t sqfs_dir_writer_get_size(const sqfs_dir_writer_t *writer)
{
	return writer->dir_size;
}

sqfs_u64 sqfs_dir_writer_get_dir_reference(const sqfs_dir_writer_t *writer)
{
	return writer->dir_ref;
}

size_t sqfs_dir_writer_get_index_size(const sqfs_dir_writer_t *writer)
{
	return writer->idx_size;
}

size_t sqfs_dir_writer_get_entry_count(const sqfs_dir_writer_t *writer)
{
	return writer->ent_count;
}

sqfs_inode_generic_t
*sqfs_dir_writer_create_inode(const sqfs_dir_writer_t *writer,
			      size_t hlinks, sqfs_u32 xattr,
			      sqfs_u32 parent_ino)
{
	sqfs_inode_generic_t *inode;
	sqfs_u64 start_block;
	sqfs_u16 block_offset;

	inode = calloc(1, sizeof(*inode));
	if (inode == NULL)
		return NULL;

	start_block = writer->dir_ref >> 16;
	block_offset = writer->dir_ref & 0xFFFF;

	if (xattr != 0xFFFFFFFF || start_block > 0xFFFFFFFFUL ||
	    writer->dir_size > 0xFFFF) {
		inode->base.type = SQFS_INODE_EXT_DIR;
	} else {
		inode->base.type = SQFS_INODE_DIR;
	}

	if (inode->base.type == SQFS_INODE_DIR) {
		inode->data.dir.start_block = start_block;
		inode->data.dir.nlink = writer->ent_count + hlinks + 2;
		inode->data.dir.size = writer->dir_size;
		inode->data.dir.offset = block_offset;
		inode->data.dir.parent_inode = parent_ino;
	} else {
		inode->data.dir_ext.nlink = writer->ent_count + hlinks + 2;
		inode->data.dir_ext.size = writer->dir_size;
		inode->data.dir_ext.start_block = start_block;
		inode->data.dir_ext.parent_inode = parent_ino;
		inode->data.dir_ext.offset = block_offset;
		inode->data.dir_ext.xattr_idx = xattr;
		inode->data.dir_ext.inodex_count =
			writer->idx_size ? (writer->idx_size - 1) : 0;
	}

	return inode;
}

int sqfs_dir_writer_write_index(const sqfs_dir_writer_t *writer,
				sqfs_meta_writer_t *im)
{
	sqfs_dir_index_t ent;
	index_ent_t *idx;
	int err;

	for (idx = writer->idx; idx != NULL; idx = idx->next) {
		ent.start_block = htole32(idx->block);
		ent.index = htole32(idx->index);
		ent.size = htole32(idx->ent->name_len - 1);

		err = sqfs_meta_writer_append(im, &ent, sizeof(ent));
		if (err)
			return err;

		err = sqfs_meta_writer_append(im, idx->ent->name,
					      idx->ent->name_len);
		if (err)
			return err;
	}

	return 0;
}
