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
#include "fstream.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef struct sparse_map_t {
	struct sparse_map_t *next;
	sqfs_u64 offset;
	sqfs_u64 count;
} sparse_map_t;

typedef struct {
	char name[100];
	char mode[8];
	char uid[8];
	char gid[8];
	char size[12];
	char mtime[12];
	char chksum[8];
	char typeflag;
	char linkname[100];
	char magic[6];
	char version[2];
	char uname[32];
	char gname[32];
	char devmajor[8];
	char devminor[8];
	union {
		struct {
			char prefix[155];
			char padding[12];
		} posix;

		struct {
			char atime[12];
			char ctime[12];
			char offset[12];
			char deprecated[4];
			char unused;
			struct {
				char offset[12];
				char numbytes[12];
			} sparse[4];
			char isextended;
			char realsize[12];
			char padding[17];
		} gnu;
	} tail;
} tar_header_t;

typedef struct {
	struct {
		char offset[12];
		char numbytes[12];
	} sparse[21];
	char isextended;
	char padding[7];
} gnu_sparse_t;

typedef struct tar_xattr_t {
	struct tar_xattr_t *next;
	char *key;
	sqfs_u8 *value;
	size_t value_len;
	char data[];
} tar_xattr_t;

typedef struct {
	struct stat sb;
	char *name;
	char *link_target;
	sparse_map_t *sparse;
	sqfs_u64 actual_size;
	sqfs_u64 record_size;
	bool unknown_record;
	bool is_hard_link;
	tar_xattr_t *xattr;

	/* broken out since struct stat could contain
	   32 bit values on 32 bit systems. */
	sqfs_s64 mtime;
} tar_header_decoded_t;

#define TAR_TYPE_FILE '0'
#define TAR_TYPE_LINK '1'
#define TAR_TYPE_SLINK '2'
#define TAR_TYPE_CHARDEV '3'
#define TAR_TYPE_BLOCKDEV '4'
#define TAR_TYPE_DIR '5'
#define TAR_TYPE_FIFO '6'

#define TAR_TYPE_GNU_SLINK 'K'
#define TAR_TYPE_GNU_PATH 'L'
#define TAR_TYPE_GNU_SPARSE 'S'

#define TAR_TYPE_PAX 'x'
#define TAR_TYPE_PAX_GLOBAL 'g'

#define TAR_MAGIC "ustar"
#define TAR_VERSION "00"

#define TAR_MAGIC_OLD "ustar "
#define TAR_VERSION_OLD " "

#define TAR_RECORD_SIZE (512)

/*
  Returns < 0 on failure, > 0 if cannot encode, 0 on success.
  Prints error/warning messages to stderr.

  The counter is an incremental record counter used if additional
  headers need to be generated.
*/
int write_tar_header(ostream_t *fp, const struct stat *sb, const char *name,
		     const char *slink_target, const tar_xattr_t *xattr,
		     unsigned int counter);

int write_hard_link(ostream_t *fp, const struct stat *sb, const char *name,
		    const char *target, unsigned int counter);

/* calcuate and skip the zero padding */
int skip_padding(istream_t *fp, sqfs_u64 size);

/* round up to block size and skip the entire entry */
int skip_entry(istream_t *fp, sqfs_u64 size);

int read_header(istream_t *fp, tar_header_decoded_t *out);

void free_xattr_list(tar_xattr_t *list);

void clear_header(tar_header_decoded_t *hdr);

/*
  Write zero bytes to an output file to padd it to the tar record size.
  Returns 0 on success. On failure, prints error message to stderr.
*/
int padd_file(ostream_t *fp, sqfs_u64 size);

#endif /* TAR_H */
