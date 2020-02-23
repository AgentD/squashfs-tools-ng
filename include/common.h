/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * common.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef COMMON_H
#define COMMON_H

#include "config.h"

#include "sqfs/xattr_writer.h"
#include "sqfs/xattr_reader.h"
#include "sqfs/compressor.h"
#include "sqfs/id_table.h"
#include "sqfs/inode.h"
#include "sqfs/table.h"
#include "sqfs/error.h"
#include "sqfs/meta_writer.h"
#include "sqfs/data_reader.h"
#include "sqfs/block_processor.h"
#include "sqfs/block_writer.h"
#include "sqfs/frag_table.h"
#include "sqfs/dir_writer.h"
#include "sqfs/dir_reader.h"
#include "sqfs/block.h"
#include "sqfs/xattr.h"
#include "sqfs/dir.h"
#include "sqfs/io.h"

#include "compat.h"
#include "fstree.h"
#include "tar.h"

#include <stddef.h>

typedef struct {
	sqfs_block_writer_t *blkwr;
	sqfs_frag_table_t *fragtbl;
	sqfs_block_processor_t *data;
	sqfs_dir_writer_t *dirwr;
	sqfs_meta_writer_t *dm;
	sqfs_meta_writer_t *im;
	sqfs_compressor_t *cmp;
	sqfs_id_table_t *idtbl;
	sqfs_file_t *outfile;
	sqfs_super_t super;
	fstree_t fs;
	sqfs_xattr_writer_t *xwr;
} sqfs_writer_t;

typedef struct {
	const char *filename;
	char *fs_defaults;
	char *comp_extra;
	size_t block_size;
	size_t devblksize;
	size_t max_backlog;
	size_t num_jobs;

	int outmode;
	E_SQFS_COMPRESSOR comp_id;

	bool exportable;
	bool no_xattr;
	bool quiet;
} sqfs_writer_cfg_t;

typedef struct sqfs_hard_link_t {
	struct sqfs_hard_link_t *next;
	sqfs_u32 inode_number;
	char *target;
} sqfs_hard_link_t;

#define container_of(ptr, type, member) \
	((type *)((char *)ptr - offsetof(type, member)))

/*
  High level helper function to serialize an entire file system tree to
  a squashfs inode table and directory table.

  The data is written to the given file descriptor and the super block is
  update accordingly (inode and directory table start and total size).

  The function internally creates two meta data writers and uses
  meta_writer_write_inode to serialize the inode table of the fstree.

  Returns 0 on success. Prints error messages to stderr on failure.
 */
int sqfs_serialize_fstree(const char *filename, sqfs_writer_t *wr);

/* Print out fancy statistics for squashfs packing tools */
void sqfs_print_statistics(const sqfs_super_t *super,
			   const sqfs_block_processor_t *blk,
			   const sqfs_block_writer_t *wr);

void compressor_print_available(void);

E_SQFS_COMPRESSOR compressor_get_default(void);

int compressor_cfg_init_options(sqfs_compressor_config_t *cfg,
				E_SQFS_COMPRESSOR id,
				size_t block_size, char *options);

void compressor_print_help(E_SQFS_COMPRESSOR id);

int inode_stat(const sqfs_tree_node_t *node, struct stat *sb);

char *sqfs_tree_node_get_path(const sqfs_tree_node_t *node);

int sqfs_data_reader_dump(const char *name, sqfs_data_reader_t *data,
			  const sqfs_inode_generic_t *inode,
			  FILE *fp, size_t block_size, bool allow_sparse);

sqfs_file_t *sqfs_get_stdin_file(FILE *fp, const sparse_map_t *map,
				 sqfs_u64 size);

int write_data_from_file(const char *filename, sqfs_block_processor_t *data,
			 sqfs_inode_generic_t **inode,
			 sqfs_file_t *file, int flags);

void sqfs_writer_cfg_init(sqfs_writer_cfg_t *cfg);

int sqfs_writer_init(sqfs_writer_t *sqfs, const sqfs_writer_cfg_t *wrcfg);

int sqfs_writer_finish(sqfs_writer_t *sqfs, const sqfs_writer_cfg_t *cfg);

void sqfs_writer_cleanup(sqfs_writer_t *sqfs);

void sqfs_perror(const char *file, const char *action, int error_code);

int sqfs_tree_find_hard_links(const sqfs_tree_node_t *root,
			      sqfs_hard_link_t **out);

/*
  A wrapper around mkdir() that behaves like 'mkdir -p'. It tries to create
  every component of the given path and skips already existing entries.

  Returns 0 on success.
*/
int mkdir_p(const char *path);

/* A common implementation of the '--version' command line flag. */
void print_version(const char *progname);

/*
  Create an liblzo2 based LZO compressor.

  XXX: This must be in libcommon instead of libsquashfs for legal reasons.
 */
sqfs_compressor_t *lzo_compressor_create(const sqfs_compressor_config_t *cfg);

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

#endif /* COMMON_H */
