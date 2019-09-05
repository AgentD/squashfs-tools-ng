/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * xattr.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef SQFS_XATTR_H
#define SQFS_XATTR_H

#include "config.h"

#include "sqfs/compress.h"

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

typedef enum {
	SQFS_XATTR_USER = 0,
	SQFS_XATTR_TRUSTED = 1,
	SQFS_XATTR_SECURITY = 2,

	SQFS_XATTR_FLAG_OOL = 0x100,
	SQFS_XATTR_PREFIX_MASK = 0xFF,
} E_SQFS_XATTR_TYPE;

typedef struct {
	uint16_t type;
	uint16_t size;
	uint8_t key[];
} sqfs_xattr_entry_t;

typedef struct {
	uint32_t size;
	uint8_t value[];
} sqfs_xattr_value_t;

typedef struct {
	uint64_t xattr;
	uint32_t count;
	uint32_t size;
} sqfs_xattr_id_t;

typedef struct {
	uint64_t xattr_table_start;
	uint32_t xattr_ids;
	uint32_t unused;
} sqfs_xattr_id_table_t;

typedef struct sqfs_xattr_reader_t sqfs_xattr_reader_t;

#ifdef __cplusplus
extern "C" {
#endif

/* Get a prefix string from the ID or NULL if unknown */
const char *sqfs_get_xattr_prefix(E_SQFS_XATTR_TYPE id);

/* Get id from xattr key prefix or -1 if not supported */
int sqfs_get_xattr_prefix_id(const char *key);

/* Check if a given xattr key can be encoded in squashfs at all. */
bool sqfs_has_xattr(const char *key);

void sqfs_xattr_reader_destroy(sqfs_xattr_reader_t *xr);

sqfs_xattr_reader_t *sqfs_xattr_reader_create(int sqfsfd, sqfs_super_t *super,
					      compressor_t *cmp);

int sqfs_xattr_reader_get_desc(sqfs_xattr_reader_t *xr, uint32_t idx,
			       sqfs_xattr_id_t *desc);

int sqfs_xattr_reader_seek_kv(sqfs_xattr_reader_t *xr,
			      const sqfs_xattr_id_t *desc);

sqfs_xattr_value_t *sqfs_xattr_reader_read_value(sqfs_xattr_reader_t *xr,
						 const sqfs_xattr_entry_t *key);

sqfs_xattr_entry_t *sqfs_xattr_reader_read_key(sqfs_xattr_reader_t *xr);

#ifdef __cplusplus
}
#endif

#endif /* SQFS_XATTR_H */
