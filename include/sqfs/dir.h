/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * dir.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef SQFS_DIR_H
#define SQFS_DIR_H

#include "config.h"

#include "sqfs/predef.h"
#include "sqfs/meta_reader.h"
#include "sqfs/meta_writer.h"

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

typedef struct sqfs_dir_writer_t sqfs_dir_writer_t;

#ifdef __cplusplus
extern "C" {
#endif

/* Returns 0 on success. Internally prints to stderr on failure */
SQFS_API int sqfs_meta_reader_read_dir_header(sqfs_meta_reader_t *m,
					      sqfs_dir_header_t *hdr);

/* Entry can be freed with a single free() call.
   The function internally prints to stderr on failure */
SQFS_API sqfs_dir_entry_t *sqfs_meta_reader_read_dir_ent(sqfs_meta_reader_t *m);

SQFS_API sqfs_dir_writer_t *sqfs_dir_writer_create(sqfs_meta_writer_t *dm);

SQFS_API void sqfs_dir_writer_destroy(sqfs_dir_writer_t *writer);

SQFS_API int sqfs_dir_writer_begin(sqfs_dir_writer_t *writer);

SQFS_API int sqfs_dir_writer_add_entry(sqfs_dir_writer_t *writer,
				       const char *name,
				       uint32_t inode_num, uint64_t inode_ref,
				       mode_t mode);

SQFS_API int sqfs_dir_writer_end(sqfs_dir_writer_t *writer);

SQFS_API size_t sqfs_dir_writer_get_size(sqfs_dir_writer_t *writer);

SQFS_API uint64_t sqfs_dir_writer_get_dir_reference(sqfs_dir_writer_t *writer);

SQFS_API size_t sqfs_dir_writer_get_index_size(sqfs_dir_writer_t *writer);

SQFS_API int sqfs_dir_writer_write_index(sqfs_dir_writer_t *writer,
					 sqfs_meta_writer_t *im);

#ifdef __cplusplus
}
#endif

#endif /* SQFS_DIR_H */
