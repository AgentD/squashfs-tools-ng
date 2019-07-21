/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "meta_writer.h"
#include "highlevel.h"
#include "util.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static const struct {
	const char *prefix;
	E_SQFS_XATTR_TYPE type;
} xattr_types[] = {
	{ "user.", SQUASHFS_XATTR_USER },
	{ "trusted.", SQUASHFS_XATTR_TRUSTED },
	{ "security.", SQUASHFS_XATTR_SECURITY },
};

static int get_prefix(const char *key)
{
	size_t i, len;

	for (i = 0; i < sizeof(xattr_types) / sizeof(xattr_types[0]); ++i) {
		len = strlen(xattr_types[i].prefix);

		if (strncmp(key, xattr_types[i].prefix, len) == 0 &&
		    strlen(key) > len) {
			return xattr_types[i].type;
		}
	}

	return -1;
}

bool sqfs_has_xattr(const char *key)
{
	return get_prefix(key) >= 0;
}

static int write_key(meta_writer_t *mw, const char *key, tree_xattr_t *xattr)
{
	const char *orig_key = key;
	sqfs_xattr_entry_t kent;
	int type;

	type = get_prefix(key);
	if (type < 0) {
		fprintf(stderr, "unsupported xattr key '%s'\n", key);
		return -1;
	}

	key = strchr(key, '.');
	assert(key != NULL);
	++key;

	kent.type = htole16(type);
	kent.size = htole16(strlen(key));

	if (meta_writer_append(mw, &kent, sizeof(kent)))
		return -1;
	if (meta_writer_append(mw, key, strlen(key)))
		return -1;

	xattr->size += sizeof(sqfs_xattr_entry_t) + strlen(orig_key);
	return 0;
}

static int write_value(meta_writer_t *mw, const char *value,
		       tree_xattr_t *xattr)
{
	sqfs_xattr_value_t vent;

	vent.size = htole32(strlen(value));

	if (meta_writer_append(mw, &vent, sizeof(vent)))
		return -1;
	if (meta_writer_append(mw, value, strlen(value)))
		return -1;

	xattr->size += sizeof(vent) + strlen(value);
	return 0;
}

static int write_kv_pairs(fstree_t *fs, meta_writer_t *mw, tree_xattr_t *xattr)
{
	uint32_t key_idx, val_idx;
	const char *key, *value;
	size_t i;

	for (i = 0; i < xattr->num_attr; ++i) {
		key_idx = xattr->attr[i].key_index;
		val_idx = xattr->attr[i].value_index;

		key = str_table_get_string(&fs->xattr_keys, key_idx);
		value = str_table_get_string(&fs->xattr_values, val_idx);

		if (write_key(mw, key, xattr))
			return -1;

		if (write_value(mw, value, xattr))
			return -1;
	}

	return 0;
}

int write_xattr(int outfd, fstree_t *fs, sqfs_super_t *super,
		compressor_t *cmp)
{
	uint64_t kv_start, id_start, block, *tbl;
	size_t i = 0, count = 0, blocks;
	sqfs_xattr_id_table_t idtbl;
	sqfs_xattr_id_t id_ent;
	meta_writer_t *mw;
	tree_xattr_t *it;
	uint32_t offset;

	if (fs->xattr == NULL)
		return 0;

	mw = meta_writer_create(outfd, cmp, false);
	if (mw == NULL)
		return -1;

	/* write xattr key-value pairs */
	kv_start = super->bytes_used;

	for (it = fs->xattr; it != NULL; it = it->next) {
		meta_writer_get_position(mw, &it->block, &it->offset);
		it->size = 0;

		if (write_kv_pairs(fs, mw, it))
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

	tbl = calloc(sizeof(uint64_t), blocks);

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

	free(tbl);
	meta_writer_destroy(mw);
	return 0;
fail_tbl:
	free(tbl);
fail_mw:
	meta_writer_destroy(mw);
	return -1;
}
