/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * tar.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef TAR_H
#define TAR_H

#include "config.h"
#include "compat.h"
#include "io/istream.h"
#include "io/ostream.h"
#include "io/dir_iterator.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef struct sparse_map_t {
	struct sparse_map_t *next;
	sqfs_u64 offset;
	sqfs_u64 count;
} sparse_map_t;

typedef struct {
	char *name;
	char *link_target;
	sparse_map_t *sparse;
	sqfs_u64 actual_size;
	sqfs_u64 record_size;
	bool unknown_record;
	bool is_hard_link;
	sqfs_xattr_t *xattr;

	sqfs_u16 mode;
	sqfs_u64 uid;
	sqfs_u64 gid;
	sqfs_u64 devno;
	sqfs_s64 mtime;
} tar_header_decoded_t;

#ifdef __cplusplus
extern "C" {
#endif

/*
  Returns < 0 on failure, > 0 if cannot encode, 0 on success.
  Prints error/warning messages to stderr.

  The counter is an incremental record counter used if additional
  headers need to be generated.
*/
int write_tar_header(ostream_t *fp, const struct stat *sb, const char *name,
		     const char *slink_target, const sqfs_xattr_t *xattr,
		     unsigned int counter);

int write_hard_link(ostream_t *fp, const struct stat *sb, const char *name,
		    const char *target, unsigned int counter);

/* round up to block size and skip the entire entry */
int read_header(istream_t *fp, tar_header_decoded_t *out);

void clear_header(tar_header_decoded_t *hdr);

dir_iterator_t *tar_open_stream(istream_t *stream);

/*
  Write zero bytes to an output file to padd it to the tar record size.
  Returns 0 on success. On failure, prints error message to stderr.
*/
int padd_file(ostream_t *fp, sqfs_u64 size);

void free_sparse_list(sparse_map_t *sparse);

#ifdef __cplusplus
}
#endif

#endif /* TAR_H */
