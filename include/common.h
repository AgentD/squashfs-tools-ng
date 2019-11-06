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
#include "sqfs/data_writer.h"
#include "sqfs/dir_writer.h"
#include "sqfs/dir_reader.h"
#include "sqfs/block.h"
#include "sqfs/xattr.h"
#include "sqfs/dir.h"
#include "sqfs/io.h"

#include "util/compat.h"
#include "util/util.h"

#include "fstree.h"
#include "tar.h"

#include <stddef.h>

typedef struct {
	size_t file_count;
	size_t blocks_written;
	size_t frag_blocks_written;
	size_t duplicate_blocks;
	size_t sparse_blocks;
	size_t frag_count;
	size_t frag_dup;
	sqfs_u64 bytes_written;
	sqfs_u64 bytes_read;
} data_writer_stats_t;

typedef struct {
	sqfs_data_writer_t *data;
	sqfs_compressor_t *cmp;
	sqfs_id_table_t *idtbl;
	sqfs_file_t *outfile;
	sqfs_super_t super;
	fstree_t fs;
	data_writer_stats_t stats;
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

/*
  High level helper function to serialize an entire file system tree to
  a squashfs inode table and directory table.

  The data is written to the given file descriptor and the super block is
  update accordingly (inode and directory table start and total size).

  The function internally creates two meta data writers and uses
  meta_writer_write_inode to serialize the inode table of the fstree.

  Returns 0 on success. Prints error messages to stderr on failure.
 */
int sqfs_serialize_fstree(const char *filename, sqfs_file_t *file,
			  sqfs_super_t *super, fstree_t *fs,
			  sqfs_compressor_t *cmp, sqfs_id_table_t *idtbl);

/*
  Generate an NFS export table.

  Returns 0 on success. Prints error messages to stderr on failure.
 */
int write_export_table(const char *filename, sqfs_file_t *file,
		       fstree_t *fs, sqfs_super_t *super,
		       sqfs_compressor_t *cmp);

/* Print out fancy statistics for squashfs packing tools */
void sqfs_print_statistics(sqfs_super_t *super, data_writer_stats_t *stats);

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

sqfs_file_t *sqfs_get_stdin_file(const sparse_map_t *map, sqfs_u64 size);

sqfs_file_t *sqfs_get_stdout_file(void);

void register_stat_hooks(sqfs_data_writer_t *data, data_writer_stats_t *stats);

int write_data_from_file(const char *filename, sqfs_data_writer_t *data,
			 sqfs_inode_generic_t *inode,
			 sqfs_file_t *file, int flags);

void sqfs_writer_cfg_init(sqfs_writer_cfg_t *cfg);

int sqfs_writer_init(sqfs_writer_t *sqfs, const sqfs_writer_cfg_t *wrcfg);

int sqfs_writer_finish(sqfs_writer_t *sqfs, const sqfs_writer_cfg_t *cfg);

void sqfs_writer_cleanup(sqfs_writer_t *sqfs);

void sqfs_perror(const char *file, const char *action, int error_code);

bool is_filename_sane(const char *name);

/*
  A wrapper around mkdir() that behaves like 'mkdir -p'. It tries to create
  every component of the given path and skips already existing entries.

  Returns 0 on success.
*/
int mkdir_p(const char *path);

/* Returns 0 on success. On failure, prints error message to stderr. */
int pushd(const char *path);

/* Same as pushd, but the string doesn't have to be null-terminated. */
int pushdn(const char *path, size_t len);

/* Returns 0 on success. On failure, prints error message to stderr. */
int popd(void);

/*
  A common implementation of the '--version' command line flag.

  Prints out version information. The program name is extracted from the
  BSD style __progname global variable.
*/
void print_version(void);

#endif /* COMMON_H */
