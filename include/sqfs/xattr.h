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
	SQUASHFS_XATTR_USER = 0,
	SQUASHFS_XATTR_TRUSTED = 1,
	SQUASHFS_XATTR_SECURITY = 2,

	SQUASHFS_XATTR_FLAG_OOL = 0x100,
	SQUASHFS_XATTR_PREFIX_MASK = 0xFF,
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

typedef struct xattr_reader_t xattr_reader_t;

#ifdef __cplusplus
extern "C" {
#endif

/* Get a prefix string from the ID or NULL if unknown */
const char *sqfs_get_xattr_prefix(E_SQFS_XATTR_TYPE id);

/* Get id from xattr key prefix or -1 if not supported */
int sqfs_get_xattr_prefix_id(const char *key);

/* Check if a given xattr key can be encoded in squashfs at all. */
bool sqfs_has_xattr(const char *key);

void xattr_reader_destroy(xattr_reader_t *xr);

xattr_reader_t *xattr_reader_create(int sqfsfd, sqfs_super_t *super,
				    compressor_t *cmp);

int xattr_reader_get_desc(xattr_reader_t *xr, uint32_t idx,
			  sqfs_xattr_id_t *desc);

int xattr_reader_seek_kv(xattr_reader_t *xr, const sqfs_xattr_id_t *desc);

sqfs_xattr_value_t *xattr_reader_read_value(xattr_reader_t *xr,
					    const sqfs_xattr_entry_t *key);

sqfs_xattr_entry_t *xattr_reader_read_key(xattr_reader_t *xr);

#ifdef __cplusplus
}
#endif

#endif /* SQFS_XATTR_H */
