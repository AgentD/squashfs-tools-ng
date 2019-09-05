/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * xattr_reader.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "sqfs/meta_reader.h"
#include "sqfs/xattr.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

struct sqfs_xattr_reader_t {
	uint64_t xattr_start;

	size_t num_id_blocks;
	size_t num_ids;

	uint64_t *id_block_starts;

	meta_reader_t *idrd;
	meta_reader_t *kvrd;
	sqfs_super_t *super;
};

static int get_id_block_locations(sqfs_xattr_reader_t *xr, int sqfsfd,
				  sqfs_super_t *super)
{
	sqfs_xattr_id_table_t idtbl;
	size_t i;

	if (super->xattr_id_table_start >= super->bytes_used) {
		fputs("xattr ID location table is after end of filesystem\n",
		      stderr);
		return -1;
	}

	if (read_data_at("reading xattr ID location table",
			 super->xattr_id_table_start,
			 sqfsfd, &idtbl, sizeof(idtbl))) {
		return -1;
	}

	xr->xattr_start = le64toh(idtbl.xattr_table_start);
	xr->num_ids = le32toh(idtbl.xattr_ids);
	xr->num_id_blocks =
		(xr->num_ids * sizeof(sqfs_xattr_id_t)) / SQFS_META_BLOCK_SIZE;

	if ((xr->num_ids * sizeof(sqfs_xattr_id_t)) % SQFS_META_BLOCK_SIZE)
		xr->num_id_blocks += 1;

	xr->id_block_starts = alloc_array(sizeof(uint64_t), xr->num_id_blocks);
	if (xr->id_block_starts == NULL) {
		perror("allocating xattr ID location table");
		return -1;
	}

	if (read_data_at("reading xattr ID block locations",
			 super->xattr_id_table_start + sizeof(idtbl),
			 sqfsfd, xr->id_block_starts,
			 sizeof(uint64_t) * xr->num_id_blocks)) {
		goto fail;
	}

	for (i = 0; i < xr->num_id_blocks; ++i) {
		xr->id_block_starts[i] = le64toh(xr->id_block_starts[i]);

		if (xr->id_block_starts[i] > super->bytes_used) {
			fputs("found xattr ID block that is past "
			      "end of filesystem\n", stderr);
			goto fail;
		}
	}

	return 0;
fail:
	free(xr->id_block_starts);
	xr->id_block_starts = NULL;
	return -1;
}

sqfs_xattr_entry_t *sqfs_xattr_reader_read_key(sqfs_xattr_reader_t *xr)
{
	sqfs_xattr_entry_t key, *out;
	const char *prefix;
	size_t plen, total;

	if (meta_reader_read(xr->kvrd, &key, sizeof(key)))
		return NULL;

	key.type = le16toh(key.type);
	key.size = le16toh(key.size);

	prefix = sqfs_get_xattr_prefix(key.type & SQFS_XATTR_PREFIX_MASK);
	if (prefix == NULL) {
		fprintf(stderr, "found unknown xattr type %u\n",
			key.type & SQFS_XATTR_PREFIX_MASK);
		return NULL;
	}

	plen = strlen(prefix);

	if (SZ_ADD_OV(plen, key.size, &total) || SZ_ADD_OV(total, 1, &total) ||
	    SZ_ADD_OV(sizeof(*out), total, &total)) {
		errno = EOVERFLOW;
		goto fail_alloc;
	}

	out = calloc(1, total);
	if (out == NULL) {
		goto fail_alloc;
	}

	*out = key;
	memcpy(out->key, prefix, plen);

	if (meta_reader_read(xr->kvrd, out->key + plen, key.size)) {
		free(out);
		return NULL;
	}

	return out;
fail_alloc:
	perror("allocating xattr key");
	return NULL;
}

sqfs_xattr_value_t *sqfs_xattr_reader_read_value(sqfs_xattr_reader_t *xr,
						 const sqfs_xattr_entry_t *key)
{
	size_t offset, new_offset, size;
	sqfs_xattr_value_t value, *out;
	uint64_t ref, start, new_start;

	if (meta_reader_read(xr->kvrd, &value, sizeof(value)))
		return NULL;

	if (key->type & SQFS_XATTR_FLAG_OOL) {
		if (meta_reader_read(xr->kvrd, &ref, sizeof(ref)))
			return NULL;

		meta_reader_get_position(xr->kvrd, &start, &offset);

		new_start = xr->xattr_start + (ref >> 16);
		new_offset = ref & 0xFFFF;

		if (new_start > xr->super->bytes_used) {
			fputs("OOL xattr reference points past end of "
			      "filesystem\n", stderr);
			return NULL;
		}

		if (new_offset >= SQFS_META_BLOCK_SIZE) {
			fputs("OOL xattr reference points outside "
			      "metadata block\n", stderr);
			return NULL;
		}

		if (meta_reader_seek(xr->kvrd, new_start, new_offset))
			return NULL;
	}

	value.size = le32toh(value.size);

	if (SZ_ADD_OV(sizeof(*out), value.size, &size) ||
	    SZ_ADD_OV(size, 1, &size)) {
		errno = EOVERFLOW;
		goto fail_alloc;
	}

	out = calloc(1, size);
	if (out == NULL)
		goto fail_alloc;

	*out = value;

	if (meta_reader_read(xr->kvrd, out->value, value.size))
		goto fail;

	if (key->type & SQFS_XATTR_FLAG_OOL) {
		if (meta_reader_seek(xr->kvrd, start, offset))
			goto fail;
	}

	return out;
fail_alloc:
	perror("allocating xattr value");
	return NULL;
fail:
	free(out);
	return NULL;
}

int sqfs_xattr_reader_seek_kv(sqfs_xattr_reader_t *xr,
			      const sqfs_xattr_id_t *desc)
{
	uint32_t offset = desc->xattr & 0xFFFF;
	uint64_t block = xr->xattr_start + (desc->xattr >> 16);

	return meta_reader_seek(xr->kvrd, block, offset);
}

int sqfs_xattr_reader_get_desc(sqfs_xattr_reader_t *xr, uint32_t idx,
			       sqfs_xattr_id_t *desc)
{
	size_t block, offset;

	memset(desc, 0, sizeof(*desc));

	if (idx == 0xFFFFFFFF)
		return 0;

	if (xr->kvrd == NULL || xr->idrd == NULL) {
		if (idx != 0)
			goto fail_bounds;
		return 0;
	}

	if (idx >= xr->num_ids)
		goto fail_bounds;

	offset = (idx * sizeof(*desc)) % SQFS_META_BLOCK_SIZE;
	block = (idx * sizeof(*desc)) / SQFS_META_BLOCK_SIZE;

	if (meta_reader_seek(xr->idrd, xr->id_block_starts[block], offset))
		return -1;

	if (meta_reader_read(xr->idrd, desc, sizeof(*desc)))
		return -1;

	desc->xattr = le64toh(desc->xattr);
	desc->count = le32toh(desc->count);
	desc->size = le32toh(desc->size);
	return 0;
fail_bounds:
	fprintf(stderr, "Tried to access out of bounds "
		"xattr index: 0x%08X\n", idx);
	return -1;
}

void sqfs_xattr_reader_destroy(sqfs_xattr_reader_t *xr)
{
	if (xr->kvrd != NULL)
		meta_reader_destroy(xr->kvrd);

	if (xr->idrd != NULL)
		meta_reader_destroy(xr->idrd);

	free(xr->id_block_starts);
	free(xr);
}

sqfs_xattr_reader_t *sqfs_xattr_reader_create(int sqfsfd, sqfs_super_t *super,
					      compressor_t *cmp)
{
	sqfs_xattr_reader_t *xr = calloc(1, sizeof(*xr));

	if (xr == NULL) {
		perror("creating xattr reader");
		return NULL;
	}

	if (super->flags & SQFS_FLAG_NO_XATTRS)
		return xr;

	if (super->xattr_id_table_start == 0xFFFFFFFFFFFFFFFF)
		return xr;

	if (get_id_block_locations(xr, sqfsfd, super))
		goto fail;

	xr->idrd = meta_reader_create(sqfsfd, cmp,
				      super->id_table_start,
				      super->bytes_used);
	if (xr->idrd == NULL)
		goto fail;

	xr->kvrd = meta_reader_create(sqfsfd, cmp,
				      super->id_table_start,
				      super->bytes_used);
	if (xr->kvrd == NULL)
		goto fail;

	xr->super = super;
	return xr;
fail:
	sqfs_xattr_reader_destroy(xr);
	return NULL;
}
