/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * meta_writer.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef META_WRITER_H
#define META_WRITER_H

#include "config.h"

#include "sqfs/compress.h"
#include "sqfs/id_table.h"
#include "sqfs/data.h"
#include "fstree.h"

typedef struct {
	tree_node_t *node;
	uint32_t block;
	uint32_t index;
} idx_ref_t;

typedef struct {
	size_t num_nodes;
	size_t max_nodes;
	idx_ref_t idx_nodes[];
} dir_index_t;

typedef struct meta_writer_t meta_writer_t;

/* Create a meta data reader using a given compressor to compress data.
   Internally prints error message to stderr on failure.
   If keep_in_mem is true, the blocks are collected in memory and must
   be explicitly flushed to disk using meta_write_write_to_file.
*/
meta_writer_t *meta_writer_create(int fd, compressor_t *cmp, bool keep_in_mem);

void meta_writer_destroy(meta_writer_t *m);

/* Compress and flush the currently unfinished block to disk. Returns 0 on
   success, internally prints error message to stderr on failure */
int meta_writer_flush(meta_writer_t *m);

/* Returns 0 on success. Prints error message to stderr on failure. */
int meta_writer_append(meta_writer_t *m, const void *data, size_t size);

/* Query the current block start position and offset within the block */
void meta_writer_get_position(const meta_writer_t *m, uint64_t *block_start,
			      uint32_t *offset);

/* Reset all internal state, including the current block start position. */
void meta_writer_reset(meta_writer_t *m);

/* If created with keep_in_mem true, write the collected blocks to disk.
   Does not flush the current block. Writes error messages to stderr and
   returns non-zero on failure. */
int meta_write_write_to_file(meta_writer_t *m);

/*
  High level helper function that writes squashfs directory entries to
  a meta data writer.

  The dir_info_t structure is used to generate the listing and updated
  accordingly (such as writing back the header position and total size).
  A directory index is created on the fly and returned in *index.
  A single free() call is sufficient.

  Returns 0 on success. Prints error messages to stderr on failure.
 */
int meta_writer_write_dir(meta_writer_t *dm, dir_info_t *dir,
			  dir_index_t **index);

/*
  High level helper function to serialize a tree_node_t to a squashfs inode
  and write it to a meta data writer.

  The inode is written to `im`. If it is a directory node, the directory
  contents are written to `dm` using meta_writer_write_dir. The given
  id_table_t is used to store the uid and gid on the fly and write the
  coresponding indices to the inode structure.

  Returns 0 on success. Prints error messages to stderr on failure.
 */
int meta_writer_write_inode(fstree_t *fs, id_table_t *idtbl, meta_writer_t *im,
			    meta_writer_t *dm, tree_node_t *node);

#endif /* META_WRITER_H */
