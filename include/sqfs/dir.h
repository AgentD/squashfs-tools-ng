/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * dir.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef SQFS_DIR_H
#define SQFS_DIR_H

#include "config.h"

#include "sqfs/meta_reader.h"

#include <stdint.h>

#define SQFS_MAX_DIR_ENT 256

typedef struct {
	uint32_t count;
	uint32_t start_block;
	uint32_t inode_number;
} sqfs_dir_header_t;

typedef struct {
	uint16_t offset;
	int16_t inode_diff;
	uint16_t type;
	uint16_t size;
	uint8_t name[];
} sqfs_dir_entry_t;

typedef struct {
	uint32_t index;
	uint32_t start_block;
	uint32_t size;
	uint8_t name[];
} sqfs_dir_index_t;

#ifdef __cplusplus
extern "C" {
#endif

/* Returns 0 on success. Internally prints to stderr on failure */
int meta_reader_read_dir_header(meta_reader_t *m, sqfs_dir_header_t *hdr);

/* Entry can be freed with a single free() call.
   The function internally prints to stderr on failure */
sqfs_dir_entry_t *meta_reader_read_dir_ent(meta_reader_t *m);

#ifdef __cplusplus
}
#endif

#endif /* SQFS_DIR_H */
