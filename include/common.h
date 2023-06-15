/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * common.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef COMMON_H
#define COMMON_H

#include "config.h"

#include "sqfs/xattr_reader.h"
#include "sqfs/inode.h"
#include "sqfs/table.h"
#include "sqfs/data_reader.h"
#include "sqfs/dir_reader.h"
#include "sqfs/block.h"
#include "sqfs/xattr.h"
#include "sqfs/dir.h"
#include "sqfs/io.h"

#include "simple_writer.h"
#include "compress_cli.h"
#include "io/std.h"
#include "compat.h"
#include "fstree.h"

#include <stddef.h>

typedef struct sqfs_hard_link_t {
	struct sqfs_hard_link_t *next;
	sqfs_u32 inode_number;
	char *target;
} sqfs_hard_link_t;

int sqfs_data_reader_dump(const char *name, sqfs_data_reader_t *data,
			  const sqfs_inode_generic_t *inode,
			  sqfs_ostream_t *fp, size_t block_size);

int write_data_from_file(const char *filename, sqfs_block_processor_t *data,
			 sqfs_inode_generic_t **inode,
			 sqfs_file_t *file, int flags);

void sqfs_perror(const char *file, const char *action, int error_code);

int sqfs_tree_find_hard_links(const sqfs_tree_node_t *root,
			      sqfs_hard_link_t **out);

/* A common implementation of the '--version' command line flag. */
void print_version(const char *progname);

/*
  Parse a number optionally followed by a KMG suffix (case insensitive). Prints
  an error message to stderr and returns -1 on failure, 0 on success.

  The "what" string is used to prefix error messages (perror style).

  If reference is non-zero, the suffix '%' can be used to compute the result as
  a multiple of the reference value.
 */
int parse_size(const char *what, size_t *out, const char *str,
	       size_t reference);

void print_size(sqfs_u64 size, char *buffer, bool round_to_int);

sqfs_ostream_t *data_writer_ostream_create(const char *filename,
					   sqfs_block_processor_t *proc,
					   sqfs_inode_generic_t **inode,
					   int flags);

/*
  Parse a comma separated list (e.g. "uid=...,gid=..." of defaults for
  fstree nodes. Used for command line parsing. Returns 0 on success,
  -1 on failure. Prints an error message to stderr on failure.
 */
int parse_fstree_defaults(fstree_defaults_t *out, char *str);

#endif /* COMMON_H */
