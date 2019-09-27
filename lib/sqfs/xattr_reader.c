/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * xattr_reader.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "config.h"

#include "sqfs/meta_reader.h"
#include "sqfs/super.h"
#include "sqfs/xattr.h"
#include "sqfs/error.h"
#include "sqfs/block.h"
#include "sqfs/io.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>

struct sqfs_xattr_reader_t {
	sqfs_u64 xattr_start;

	size_t num_id_blocks;
	size_t num_ids;

	sqfs_u64 *id_block_starts;

	sqfs_meta_reader_t *idrd;
	sqfs_meta_reader_t *kvrd;
	sqfs_super_t *super;
	sqfs_file_t *file;
};

int sqfs_xattr_reader_load_locations(sqfs_xattr_reader_t *xr)
{
	sqfs_xattr_id_table_t idtbl;
	size_t i;
	int err;

	if (xr->super->flags & SQFS_FLAG_NO_XATTRS)
		return 0;

	if (xr->super->xattr_id_table_start == 0xFFFFFFFFFFFFFFFF)
		return 0;

	if (xr->super->xattr_id_table_start >= xr->super->bytes_used)
		return SQFS_ERROR_OUT_OF_BOUNDS;

	err = xr->file->read_at(xr->file, xr->super->xattr_id_table_start,
				&idtbl, sizeof(idtbl));
	if (err)
		return err;

	xr->xattr_start = le64toh(idtbl.xattr_table_start);
	xr->num_ids = le32toh(idtbl.xattr_ids);
	xr->num_id_blocks =
		(xr->num_ids * sizeof(sqfs_xattr_id_t)) / SQFS_META_BLOCK_SIZE;

	if ((xr->num_ids * sizeof(sqfs_xattr_id_t)) % SQFS_META_BLOCK_SIZE)
		xr->num_id_blocks += 1;

	xr->id_block_starts = alloc_array(sizeof(sqfs_u64), xr->num_id_blocks);
	if (xr->id_block_starts == NULL) {
		if (errno == EOVERFLOW)
			return SQFS_ERROR_OVERFLOW;
		return SQFS_ERROR_ALLOC;
	}

	err = xr->file->read_at(xr->file,
				xr->super->xattr_id_table_start + sizeof(idtbl),
				xr->id_block_starts,
				sizeof(sqfs_u64) * xr->num_id_blocks);
	if (err)
		goto fail;

	for (i = 0; i < xr->num_id_blocks; ++i) {
		xr->id_block_starts[i] = le64toh(xr->id_block_starts[i]);

		if (xr->id_block_starts[i] > xr->super->bytes_used) {
			err = SQFS_ERROR_OUT_OF_BOUNDS;
			goto fail;
		}
	}

	return 0;
fail:
	free(xr->id_block_starts);
	xr->id_block_starts = NULL;
	return err;
}

int sqfs_xattr_reader_read_key(sqfs_xattr_reader_t *xr,
			       sqfs_xattr_entry_t **key_out)
{
	sqfs_xattr_entry_t key, *out;
	const char *prefix;
	size_t plen, total;
	int ret;

	ret = sqfs_meta_reader_read(xr->kvrd, &key, sizeof(key));
	if (ret)
		return ret;

	key.type = le16toh(key.type);
	key.size = le16toh(key.size);

	prefix = sqfs_get_xattr_prefix(key.type & SQFS_XATTR_PREFIX_MASK);
	if (prefix == NULL)
		return SQFS_ERROR_UNSUPPORTED;

	plen = strlen(prefix);

	if (SZ_ADD_OV(plen, key.size, &total) || SZ_ADD_OV(total, 1, &total) ||
	    SZ_ADD_OV(sizeof(*out), total, &total)) {
		return SQFS_ERROR_OVERFLOW;
	}

	out = calloc(1, total);
	if (out == NULL)
		return SQFS_ERROR_ALLOC;

	*out = key;
	memcpy(out->key, prefix, plen);

	ret = sqfs_meta_reader_read(xr->kvrd, out->key + plen, key.size);
	if (ret) {
		free(out);
		return ret;
	}

	*key_out = out;
	return 0;
}

int sqfs_xattr_reader_read_value(sqfs_xattr_reader_t *xr,
				 const sqfs_xattr_entry_t *key,
				 sqfs_xattr_value_t **val_out)
{
	size_t offset, new_offset, size;
	sqfs_xattr_value_t value, *out;
	sqfs_u64 ref, start, new_start;
	int ret;

	ret = sqfs_meta_reader_read(xr->kvrd, &value, sizeof(value));
	if (ret)
		return ret;

	if (key->type & SQFS_XATTR_FLAG_OOL) {
		ret = sqfs_meta_reader_read(xr->kvrd, &ref, sizeof(ref));
		if (ret)
			return ret;

		sqfs_meta_reader_get_position(xr->kvrd, &start, &offset);

		new_start = xr->xattr_start + (ref >> 16);
		if (new_start >= xr->super->bytes_used)
			return SQFS_ERROR_OUT_OF_BOUNDS;

		new_offset = ref & 0xFFFF;
		if (new_offset >= SQFS_META_BLOCK_SIZE)
			return SQFS_ERROR_OUT_OF_BOUNDS;

		ret = sqfs_meta_reader_seek(xr->kvrd, new_start, new_offset);
		if (ret)
			return ret;
	}

	value.size = le32toh(value.size);

	if (SZ_ADD_OV(sizeof(*out), value.size, &size) ||
	    SZ_ADD_OV(size, 1, &size)) {
		return SQFS_ERROR_OVERFLOW;
	}

	out = calloc(1, size);
	if (out == NULL)
		return SQFS_ERROR_ALLOC;

	*out = value;

	ret = sqfs_meta_reader_read(xr->kvrd, out->value, value.size);
	if (ret)
		goto fail;

	if (key->type & SQFS_XATTR_FLAG_OOL) {
		ret = sqfs_meta_reader_seek(xr->kvrd, start, offset);
		if (ret)
			goto fail;
	}

	*val_out = out;
	return 0;
fail:
	free(out);
	return ret;
}

int sqfs_xattr_reader_seek_kv(sqfs_xattr_reader_t *xr,
			      const sqfs_xattr_id_t *desc)
{
	sqfs_u32 offset = desc->xattr & 0xFFFF;
	sqfs_u64 block = xr->xattr_start + (desc->xattr >> 16);

	return sqfs_meta_reader_seek(xr->kvrd, block, offset);
}

int sqfs_xattr_reader_get_desc(sqfs_xattr_reader_t *xr, sqfs_u32 idx,
			       sqfs_xattr_id_t *desc)
{
	size_t block, offset;
	int ret;

	memset(desc, 0, sizeof(*desc));

	if (idx == 0xFFFFFFFF)
		return 0;

	if (xr->kvrd == NULL || xr->idrd == NULL)
		return idx == 0 ? 0 : SQFS_ERROR_OUT_OF_BOUNDS;

	if (idx >= xr->num_ids)
		return SQFS_ERROR_OUT_OF_BOUNDS;

	offset = (idx * sizeof(*desc)) % SQFS_META_BLOCK_SIZE;
	block = (idx * sizeof(*desc)) / SQFS_META_BLOCK_SIZE;

	ret = sqfs_meta_reader_seek(xr->idrd, xr->id_block_starts[block],
				    offset);
	if (ret)
		return ret;

	ret = sqfs_meta_reader_read(xr->idrd, desc, sizeof(*desc));
	if (ret)
		return ret;

	desc->xattr = le64toh(desc->xattr);
	desc->count = le32toh(desc->count);
	desc->size = le32toh(desc->size);
	return 0;
}

void sqfs_xattr_reader_destroy(sqfs_xattr_reader_t *xr)
{
	if (xr->kvrd != NULL)
		sqfs_meta_reader_destroy(xr->kvrd);

	if (xr->idrd != NULL)
		sqfs_meta_reader_destroy(xr->idrd);

	free(xr->id_block_starts);
	free(xr);
}

sqfs_xattr_reader_t *sqfs_xattr_reader_create(sqfs_file_t *file,
					      sqfs_super_t *super,
					      sqfs_compressor_t *cmp)
{
	sqfs_xattr_reader_t *xr = calloc(1, sizeof(*xr));

	if (xr == NULL)
		return NULL;

	xr->file = file;
	xr->super = super;

	if (super->flags & SQFS_FLAG_NO_XATTRS)
		return xr;

	if (super->xattr_id_table_start == 0xFFFFFFFFFFFFFFFF)
		return xr;

	xr->idrd = sqfs_meta_reader_create(file, cmp,
					   super->id_table_start,
					   super->bytes_used);
	if (xr->idrd == NULL)
		goto fail;

	xr->kvrd = sqfs_meta_reader_create(file, cmp,
					   super->id_table_start,
					   super->bytes_used);
	if (xr->kvrd == NULL)
		goto fail;

	return xr;
fail:
	sqfs_xattr_reader_destroy(xr);
	return NULL;
}
