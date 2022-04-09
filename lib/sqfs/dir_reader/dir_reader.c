/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * fs_reader.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "internal.h"

static void dir_reader_destroy(sqfs_object_t *obj)
{
	sqfs_dir_reader_t *rd = (sqfs_dir_reader_t *)obj;

	sqfs_dir_reader_dcache_cleanup(rd);
	sqfs_destroy(rd->meta_inode);
	sqfs_destroy(rd->meta_dir);
	free(rd);
}

static sqfs_object_t *dir_reader_copy(const sqfs_object_t *obj)
{
	const sqfs_dir_reader_t *rd = (const sqfs_dir_reader_t *)obj;
	sqfs_dir_reader_t *copy  = malloc(sizeof(*copy));

	if (copy == NULL)
		return NULL;

	memcpy(copy, rd, sizeof(*copy));

	if (sqfs_dir_reader_dcache_init_copy(copy, rd))
		goto fail_cache;

	copy->meta_inode = sqfs_copy(rd->meta_inode);
	if (copy->meta_inode == NULL)
		goto fail_mino;

	copy->meta_dir = sqfs_copy(rd->meta_dir);
	if (copy->meta_dir == NULL)
		goto fail_mdir;

	return (sqfs_object_t *)copy;
fail_mdir:
	sqfs_destroy(copy->meta_inode);
fail_mino:
	sqfs_dir_reader_dcache_cleanup(copy);
fail_cache:
	free(copy);
	return NULL;
}

sqfs_dir_reader_t *sqfs_dir_reader_create(const sqfs_super_t *super,
					  sqfs_compressor_t *cmp,
					  sqfs_file_t *file,
					  sqfs_u32 flags)
{
	sqfs_dir_reader_t *rd;
	sqfs_u64 start, limit;

	if (flags & ~SQFS_DIR_READER_ALL_FLAGS)
		return NULL;

	rd = calloc(1, sizeof(*rd));
	if (rd == NULL)
		return NULL;

	if (sqfs_dir_reader_dcache_init(rd, flags))
		goto fail_dcache;

	start = super->inode_table_start;
	limit = super->directory_table_start;

	rd->meta_inode = sqfs_meta_reader_create(file, cmp, start, limit);
	if (rd->meta_inode == NULL)
		goto fail_mino;

	start = super->directory_table_start;
	limit = super->id_table_start;

	if (super->fragment_table_start < limit)
		limit = super->fragment_table_start;

	if (super->export_table_start < limit)
		limit = super->export_table_start;

	rd->meta_dir = sqfs_meta_reader_create(file, cmp, start, limit);
	if (rd->meta_dir == NULL)
		goto fail_mdir;

	((sqfs_object_t *)rd)->destroy = dir_reader_destroy;
	((sqfs_object_t *)rd)->copy = dir_reader_copy;
	rd->super = super;
	rd->flags = flags;
	rd->state = DIR_STATE_NONE;
	return rd;
fail_mdir:
	sqfs_destroy(rd->meta_inode);
fail_mino:
	sqfs_dir_reader_dcache_cleanup(rd);
fail_dcache:
	free(rd);
	return NULL;
}

int sqfs_dir_reader_open_dir(sqfs_dir_reader_t *rd,
			     const sqfs_inode_generic_t *inode,
			     sqfs_u32 flags)
{
	sqfs_u64 block_start;
	size_t size, offset;
	sqfs_u32 parent;

	if (flags & (~SQFS_DIR_OPEN_ALL_FLAGS))
		return SQFS_ERROR_UNSUPPORTED;

	if (inode->base.type == SQFS_INODE_DIR) {
		parent = inode->data.dir.parent_inode;
		size = inode->data.dir.size;
		offset = inode->data.dir.offset;
		block_start = inode->data.dir.start_block;
	} else if (inode->base.type == SQFS_INODE_EXT_DIR) {
		parent = inode->data.dir_ext.parent_inode;
		size = inode->data.dir_ext.size;
		offset = inode->data.dir_ext.offset;
		block_start = inode->data.dir_ext.start_block;
	} else {
		return SQFS_ERROR_NOT_DIR;
	}

	if ((rd->flags & SQFS_DIR_READER_DOT_ENTRIES) &&
	    !(flags & SQFS_DIR_OPEN_NO_DOT_ENTRIES)) {
		if (sqfs_dir_reader_dcache_find(rd, inode->base.inode_number,
						&rd->cur_ref)) {
			return SQFS_ERROR_NO_ENTRY;
		}

		if (rd->cur_ref == rd->super->root_inode_ref) {
			rd->parent_ref = rd->cur_ref;
		} else if (sqfs_dir_reader_dcache_find(rd, parent,
						       &rd->parent_ref)) {
			return SQFS_ERROR_NO_ENTRY;
		}

		rd->state = DIR_STATE_OPENED;
	} else {
		rd->state = DIR_STATE_ENTRIES;
	}

	rd->start_state = rd->state;

	memset(&rd->hdr, 0, sizeof(rd->hdr));
	rd->size = size;
	rd->entries = 0;

	block_start += rd->super->directory_table_start;

	rd->dir_block_start = block_start;
	rd->dir_offset = offset;
	rd->start_size = size;

	if (rd->size <= sizeof(rd->hdr))
		return 0;

	return sqfs_meta_reader_seek(rd->meta_dir, block_start, offset);
}

static int mk_dummy_entry(const char *str, sqfs_dir_entry_t **out)
{
	size_t len = strlen(str);
	sqfs_dir_entry_t *ent;

	ent = calloc(1, sizeof(sqfs_dir_entry_t) + len + 1);
	if (ent == NULL)
		return SQFS_ERROR_ALLOC;

	ent->type = SQFS_INODE_DIR;
	ent->size = len - 1;

	strcpy((char *)ent->name, str);

	*out = ent;
	return 0;
}

int sqfs_dir_reader_read(sqfs_dir_reader_t *rd, sqfs_dir_entry_t **out)
{
	sqfs_dir_entry_t *ent;
	size_t count;
	int err;

	switch (rd->state) {
	case DIR_STATE_OPENED:
		err = mk_dummy_entry(".", out);
		if (err == 0)
			rd->state = DIR_STATE_DOT;
		return err;
	case DIR_STATE_DOT:
		err = mk_dummy_entry("..", out);
		if (err == 0)
			rd->state = DIR_STATE_DOT_DOT;
		return err;
	case DIR_STATE_DOT_DOT:
		rd->state = DIR_STATE_ENTRIES;
		break;
	case DIR_STATE_ENTRIES:
		break;
	default:
		return SQFS_ERROR_SEQUENCE;
	}

	if (!rd->entries) {
		if (rd->size <= sizeof(rd->hdr))
			return 1;

		err = sqfs_meta_reader_read_dir_header(rd->meta_dir, &rd->hdr);
		if (err)
			return err;

		rd->size -= sizeof(rd->hdr);
		rd->entries = rd->hdr.count + 1;
	}

	if (rd->size <= sizeof(*ent)) {
		rd->size = 0;
		rd->entries = 0;
		return 1;
	}

	err = sqfs_meta_reader_read_dir_ent(rd->meta_dir, &ent);
	if (err)
		return err;

	count = sizeof(*ent) + strlen((const char *)ent->name);

	if (count > rd->size) {
		rd->size = 0;
		rd->entries = 0;
	} else {
		rd->size -= count;
		rd->entries -= 1;
	}

	rd->inode_offset = ent->offset;
	*out = ent;
	return 0;
}

int sqfs_dir_reader_rewind(sqfs_dir_reader_t *rd)
{
	if (rd->state == DIR_STATE_NONE)
		return SQFS_ERROR_SEQUENCE;

	memset(&rd->hdr, 0, sizeof(rd->hdr));
	rd->size = rd->start_size;
	rd->entries = 0;
	rd->state = rd->start_state;

	if (rd->size <= sizeof(rd->hdr))
		return 0;

	return sqfs_meta_reader_seek(rd->meta_dir, rd->dir_block_start,
				     rd->dir_offset);
}

int sqfs_dir_reader_find(sqfs_dir_reader_t *rd, const char *name)
{
	sqfs_dir_entry_t *ent;
	int ret;

	if (rd->state == DIR_STATE_NONE)
		return SQFS_ERROR_SEQUENCE;

	if (rd->size != rd->start_size || rd->state != rd->start_state) {
		ret = sqfs_dir_reader_rewind(rd);
		if (ret)
			return ret;
	}

	do {
		ret = sqfs_dir_reader_read(rd, &ent);
		if (ret < 0)
			return ret;
		if (ret > 0)
			return SQFS_ERROR_NO_ENTRY;

		ret = strcmp((const char *)ent->name, name);
		free(ent);
	} while (ret < 0);

	return ret == 0 ? 0 : SQFS_ERROR_NO_ENTRY;
}

int sqfs_dir_reader_get_inode(sqfs_dir_reader_t *rd,
			      sqfs_inode_generic_t **inode)
{
	sqfs_u64 block_start;
	sqfs_u16 offset;
	int ret;

	switch (rd->state) {
	case DIR_STATE_DOT:
		block_start = rd->cur_ref >> 16;
		offset = rd->cur_ref & 0x0FFFF;
		break;
	case DIR_STATE_DOT_DOT:
		block_start = rd->parent_ref >> 16;
		offset = rd->parent_ref & 0x0FFFF;
		break;
	case DIR_STATE_ENTRIES:
		block_start = rd->hdr.start_block;
		offset = rd->inode_offset;
		break;
	default:
		return SQFS_ERROR_SEQUENCE;
	}

	ret = sqfs_meta_reader_read_inode(rd->meta_inode, rd->super,
					  block_start, offset, inode);
	if (ret != 0)
		return ret;

	if ((*inode)->base.type == SQFS_INODE_DIR ||
	    (*inode)->base.type == SQFS_INODE_EXT_DIR) {
		sqfs_u32 inum = (*inode)->base.inode_number;
		sqfs_u64 ref = (block_start << 16) | rd->inode_offset;

		ret = sqfs_dir_reader_dcache_add(rd, inum, ref);
		if (ret != 0)
			return ret;
	}

	return 0;
}

int sqfs_dir_reader_get_root_inode(sqfs_dir_reader_t *rd,
				   sqfs_inode_generic_t **inode)
{
	sqfs_u64 block_start = rd->super->root_inode_ref >> 16;
	sqfs_u16 offset = rd->super->root_inode_ref & 0xFFFF;
	int ret;

	ret = sqfs_meta_reader_read_inode(rd->meta_inode, rd->super,
					  block_start, offset, inode);

	if (ret != 0)
		return ret;

	if ((*inode)->base.type == SQFS_INODE_DIR ||
	    (*inode)->base.type == SQFS_INODE_EXT_DIR) {
		sqfs_u32 inum = (*inode)->base.inode_number;
		sqfs_u64 ref = rd->super->root_inode_ref;

		ret = sqfs_dir_reader_dcache_add(rd, inum, ref);
		if (ret != 0)
			return ret;
	}

	return 0;
}
