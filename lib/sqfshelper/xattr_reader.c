/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * xattr_reader.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "sqfs/meta_reader.h"
#include "sqfs/xattr.h"
#include "xattr_reader.h"
#include "util.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

struct xattr_reader_t {
	uint64_t xattr_start;

	size_t num_id_blocks;
	size_t num_ids;

	uint64_t *id_block_starts;

	meta_reader_t *idrd;
	meta_reader_t *kvrd;
	sqfs_super_t *super;
};

static int get_id_block_locations(xattr_reader_t *xr, int sqfsfd,
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

static int get_xattr_desc(xattr_reader_t *xr, uint32_t idx,
			  sqfs_xattr_id_t *desc)
{
	size_t block, offset;

	if (idx >= xr->num_ids) {
		fprintf(stderr, "Tried to access out of bounds "
			"xattr index: 0x%08X\n", idx);
		return -1;
	}

	offset = (idx * sizeof(*desc)) % SQFS_META_BLOCK_SIZE;
	block = (idx * sizeof(*desc)) / SQFS_META_BLOCK_SIZE;

	if (meta_reader_seek(xr->idrd, xr->id_block_starts[block], offset))
		return -1;

	if (meta_reader_read(xr->idrd, desc, sizeof(*desc)))
		return -1;

	desc->xattr = le64toh(desc->xattr);
	desc->count = le32toh(desc->count);
	desc->size = le32toh(desc->size);

	if ((desc->xattr & 0xFFFF) >= SQFS_META_BLOCK_SIZE) {
		fputs("Found xattr ID record pointing outside "
		      "metadata block\n", stderr);
		return -1;
	}

	if ((xr->xattr_start + (desc->xattr >> 16)) >= xr->super->bytes_used) {
		fputs("Found xattr ID record pointing past "
		      "end of filesystem\n", stderr);
		return -1;
	}

	return 0;
}

static sqfs_xattr_entry_t *read_key(xattr_reader_t *xr)
{
	sqfs_xattr_entry_t key, *out;
	const char *prefix;
	size_t plen, total;

	if (meta_reader_read(xr->kvrd, &key, sizeof(key)))
		return NULL;

	key.type = le16toh(key.type);
	key.size = le16toh(key.size);

	prefix = sqfs_get_xattr_prefix(key.type & SQUASHFS_XATTR_PREFIX_MASK);
	if (prefix == NULL) {
		fprintf(stderr, "found unknown xattr type %u\n",
			key.type & SQUASHFS_XATTR_PREFIX_MASK);
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

static sqfs_xattr_value_t *read_value(xattr_reader_t *xr,
				      const sqfs_xattr_entry_t *key)
{
	size_t offset, new_offset, size;
	sqfs_xattr_value_t value, *out;
	uint64_t ref, start, new_start;

	if (meta_reader_read(xr->kvrd, &value, sizeof(value)))
		return NULL;

	if (key->type & SQUASHFS_XATTR_FLAG_OOL) {
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

	if (key->type & SQUASHFS_XATTR_FLAG_OOL) {
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

static int restore_kv_pairs(xattr_reader_t *xr, fstree_t *fs,
			    tree_node_t *node)
{
	size_t i, key_idx, val_idx;
	sqfs_xattr_entry_t *key;
	sqfs_xattr_value_t *val;
	int ret;

	if (meta_reader_seek(xr->kvrd, node->xattr->block,
			     node->xattr->offset)) {
		return -1;
	}

	for (i = 0; i < node->xattr->num_attr; ++i) {
		key = read_key(xr);
		if (key == NULL)
			return -1;

		val = read_value(xr, key);
		if (val == NULL)
			goto fail_key;

		ret = str_table_get_index(&fs->xattr_keys,
					  (const char *)key->key, &key_idx);
		if (ret)
			goto fail_kv;

		ret = str_table_get_index(&fs->xattr_values,
					  (const char *)val->value, &val_idx);
		if (ret)
			goto fail_kv;

		if (sizeof(size_t) > sizeof(uint32_t)) {
			if (key_idx > 0xFFFFFFFFUL) {
				fputs("too many unique xattr keys\n", stderr);
				goto fail_kv;
			}

			if (val_idx > 0xFFFFFFFFUL) {
				fputs("too many unique xattr values\n", stderr);
				goto fail_kv;
			}
		}

		node->xattr->attr[i].key_index = key_idx;
		node->xattr->attr[i].value_index = val_idx;

		free(key);
		free(val);
	}

	return 0;
fail_kv:
	free(val);
fail_key:
	free(key);
	return -1;
}

int xattr_reader_restore_node(xattr_reader_t *xr, fstree_t *fs,
			      tree_node_t *node, uint32_t xattr)
{
	sqfs_xattr_id_t desc;
	tree_xattr_t *it;

	if (xr->kvrd == NULL || xr->idrd == NULL)
		return 0;

	if (xattr == 0xFFFFFFFF)
		return 0;

	for (it = fs->xattr; it != NULL; it = it->next) {
		if (it->index == xattr) {
			node->xattr = it;
			return 0;
		}
	}

	if (get_xattr_desc(xr, xattr, &desc))
		return -1;

	node->xattr = alloc_flex(sizeof(*node->xattr),
				 sizeof(node->xattr->attr[0]), desc.count);
	if (node->xattr == NULL) {
		perror("creating xattr structure");
		return -1;
	}

	node->xattr->num_attr = desc.count;
	node->xattr->max_attr = desc.count;
	node->xattr->block = xr->xattr_start + (desc.xattr >> 16);
	node->xattr->offset = desc.xattr & 0xFFFF;
	node->xattr->size = desc.size;
	node->xattr->index = xattr;
	node->xattr->owner = node;

	if (restore_kv_pairs(xr, fs, node)) {
		free(node->xattr);
		return -1;
	}

	node->xattr->next = fs->xattr;
	fs->xattr = node->xattr;
	return 0;
}

void xattr_reader_destroy(xattr_reader_t *xr)
{
	if (xr->kvrd != NULL)
		meta_reader_destroy(xr->kvrd);

	if (xr->idrd != NULL)
		meta_reader_destroy(xr->idrd);

	free(xr->id_block_starts);
	free(xr);
}

xattr_reader_t *xattr_reader_create(int sqfsfd, sqfs_super_t *super,
				    compressor_t *cmp)
{
	xattr_reader_t *xr = calloc(1, sizeof(*xr));

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
	xattr_reader_destroy(xr);
	return NULL;
}
