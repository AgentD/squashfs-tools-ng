/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * write_xattr.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "meta_writer.h"
#include "highlevel.h"
#include "util.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static int write_key(meta_writer_t *mw, const char *key, tree_xattr_t *xattr,
		     bool value_is_ool)
{
	sqfs_xattr_entry_t kent;
	int type;

	type = sqfs_get_xattr_prefix_id(key);
	if (type < 0) {
		fprintf(stderr, "unsupported xattr key '%s'\n", key);
		return -1;
	}

	key = strchr(key, '.');
	assert(key != NULL);
	++key;

	if (value_is_ool)
		type |= SQUASHFS_XATTR_FLAG_OOL;

	kent.type = htole16(type);
	kent.size = htole16(strlen(key));

	if (meta_writer_append(mw, &kent, sizeof(kent)))
		return -1;
	if (meta_writer_append(mw, key, strlen(key)))
		return -1;

	xattr->size += sizeof(sqfs_xattr_entry_t) + strlen(key);
	return 0;
}

static int write_value(meta_writer_t *mw, const char *value,
		       tree_xattr_t *xattr, uint64_t *value_ref_out)
{
	sqfs_xattr_value_t vent;
	uint32_t offset;
	uint64_t block;

	meta_writer_get_position(mw, &block, &offset);
	*value_ref_out = (block << 16) | (offset & 0xFFFF);

	vent.size = htole32(strlen(value));

	if (meta_writer_append(mw, &vent, sizeof(vent)))
		return -1;

	if (meta_writer_append(mw, value, strlen(value)))
		return -1;

	xattr->size += sizeof(vent) + strlen(value);
	return 0;
}

static int write_value_ool(meta_writer_t *mw, uint64_t location,
			   tree_xattr_t *xattr)
{
	sqfs_xattr_value_t vent;
	uint64_t ref;

	vent.size = htole32(sizeof(location));
	if (meta_writer_append(mw, &vent, sizeof(vent)))
		return -1;

	ref = htole64(location);
	if (meta_writer_append(mw, &ref, sizeof(ref)))
		return -1;

	xattr->size += sizeof(vent) + sizeof(ref);
	return 0;
}

static bool should_store_ool(fstree_t *fs, const char *value, size_t index)
{
	size_t refcount;

	refcount = str_table_get_ref_count(&fs->xattr_values, index);
	if (refcount < 2)
		return false;

	/*
	  Storing in line needs this many bytes: refcount * len

	  Storing out-of-line needs this many: len + (refcount - 1) * 8

	  Out-of-line prefereable if:
	      refcount * len > len + (refcount - 1) * 8
	   => refcount * len - len > (refcount - 1) * 8
	   => (refcount - 1) * len > (refcount - 1) * 8
	   => len > 8

	   Note that this only holds iff refcount - 1 != 0, i.e. refcount > 1,
	   otherwise we would be dividing by 0 in the 3rd step.
	 */
	return strlen(value) > sizeof(uint64_t);
}

static int write_kv_pairs(fstree_t *fs, meta_writer_t *mw, tree_xattr_t *xattr,
			  uint64_t *ool_locations)
{
	uint32_t key_idx, val_idx;
	const char *key, *value;
	uint64_t ref;
	size_t i;

	for (i = 0; i < xattr->num_attr; ++i) {
		key_idx = xattr->attr[i].key_index;
		val_idx = xattr->attr[i].value_index;

		key = str_table_get_string(&fs->xattr_keys, key_idx);
		value = str_table_get_string(&fs->xattr_values, val_idx);

		if (ool_locations[val_idx] == 0xFFFFFFFFFFFFFFFF) {
			if (write_key(mw, key, xattr, false))
				return -1;

			if (write_value(mw, value, xattr, &ref))
				return -1;

			if (should_store_ool(fs, value, val_idx))
				ool_locations[val_idx] = ref;
		} else {
			if (write_key(mw, key, xattr, true))
				return -1;

			if (write_value_ool(mw, ool_locations[val_idx], xattr))
				return -1;
		}
	}

	return 0;
}

static uint64_t *create_ool_locations_table(fstree_t *fs)
{
	uint64_t *table;
	size_t i;

	table = alloc_array(sizeof(uint64_t), fs->xattr_values.num_strings);

	if (table == NULL) {
		perror("allocating Xattr OOL locations table");
		return NULL;
	}

	for (i = 0; i < fs->xattr_values.num_strings; ++i) {
		table[i] = 0xFFFFFFFFFFFFFFFFUL;
	}

	return table;
}

int write_xattr(int outfd, fstree_t *fs, sqfs_super_t *super,
		compressor_t *cmp)
{
	uint64_t kv_start, id_start, block, *tbl, *ool_locations;
	size_t i = 0, count = 0, blocks;
	sqfs_xattr_id_table_t idtbl;
	sqfs_xattr_id_t id_ent;
	meta_writer_t *mw;
	tree_xattr_t *it;
	uint32_t offset;

	if (fs->xattr == NULL)
		return 0;

	ool_locations = create_ool_locations_table(fs);
	if (ool_locations == NULL)
		return -1;

	mw = meta_writer_create(outfd, cmp, false);
	if (mw == NULL)
		goto fail_ool;

	/* write xattr key-value pairs */
	kv_start = super->bytes_used;

	for (it = fs->xattr; it != NULL; it = it->next) {
		meta_writer_get_position(mw, &it->block, &it->offset);
		it->size = 0;

		if (write_kv_pairs(fs, mw, it, ool_locations))
			goto fail_mw;

		++count;
	}

	if (meta_writer_flush(mw))
		goto fail_mw;

	meta_writer_get_position(mw, &block, &offset);
	meta_writer_reset(mw);

	super->bytes_used += block;

	/* allocate location table */
	blocks = (count * sizeof(id_ent)) / SQFS_META_BLOCK_SIZE;

	if ((count * sizeof(id_ent)) % SQFS_META_BLOCK_SIZE)
		++blocks;

	tbl = alloc_array(sizeof(uint64_t), blocks);

	if (tbl == NULL) {
		perror("generating xattr ID table");
		goto fail_mw;
	}

	/* write ID table referring to key value pairs, record offsets */
	id_start = 0;
	tbl[i++] = htole64(super->bytes_used);

	for (it = fs->xattr; it != NULL; it = it->next) {
		id_ent.xattr = htole64((it->block << 16) | it->offset);
		id_ent.count = htole32(it->num_attr);
		id_ent.size = htole32(it->size);

		if (meta_writer_append(mw, &id_ent, sizeof(id_ent)))
			goto fail_tbl;

		meta_writer_get_position(mw, &block, &offset);

		if (block != id_start) {
			id_start = block;
			tbl[i++] = htole64(super->bytes_used + id_start);
		}
	}

	if (meta_writer_flush(mw))
		goto fail_tbl;

	meta_writer_get_position(mw, &block, &offset);
	super->bytes_used += block;

	/* write offset table */
	idtbl.xattr_table_start = htole64(kv_start);
	idtbl.xattr_ids = htole32(count);
	idtbl.unused = 0;

	if (write_data("writing xattr ID table", outfd, &idtbl, sizeof(idtbl)))
		goto fail_tbl;

	if (write_data("writing xattr ID table",
		       outfd, tbl, sizeof(tbl[0]) * blocks)) {
		goto fail_tbl;
	}

	super->xattr_id_table_start = super->bytes_used;
	super->bytes_used += sizeof(idtbl) + sizeof(tbl[0]) * blocks;
	super->flags &= ~SQFS_FLAG_NO_XATTRS;

	free(tbl);
	meta_writer_destroy(mw);
	free(ool_locations);
	return 0;
fail_tbl:
	free(tbl);
fail_mw:
	meta_writer_destroy(mw);
fail_ool:
	free(ool_locations);
	return -1;
}
