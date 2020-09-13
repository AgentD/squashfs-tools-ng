/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * internal.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef INTERNAL_H
#define INTERNAL_H

#include "config.h"

#include "tar.h"

#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <ctype.h>
#include <stdio.h>

enum {
	PAX_SIZE = 0x001,
	PAX_UID = 0x002,
	PAX_GID = 0x004,
	PAX_DEV_MAJ = 0x008,
	PAX_DEV_MIN = 0x010,
	PAX_NAME = 0x020,
	PAX_SLINK_TARGET = 0x040,
	PAX_MTIME = 0x100,
	PAX_SPARSE_SIZE = 0x400,

	PAX_SPARSE_GNU_1_X = 0x800,
};

enum {
	ETV_UNKNOWN = 0,
	ETV_V7_UNIX,
	ETV_PRE_POSIX,
	ETV_POSIX,
};


#define TAR_MAX_SYMLINK_LEN (65536)
#define TAR_MAX_PATH_LEN (65536)
#define TAR_MAX_PAX_LEN (65536)
#define TAR_MAX_SPARSE_ENT (65536)


int read_octal(const char *str, int digits, sqfs_u64 *out);

int read_binary(const char *str, int digits, sqfs_u64 *out);

int read_number(const char *str, int digits, sqfs_u64 *out);

int pax_read_decimal(const char *str, sqfs_u64 *out);

void update_checksum(tar_header_t *hdr);

bool is_checksum_valid(const tar_header_t *hdr);

sparse_map_t *read_sparse_map(const char *line);

sparse_map_t *read_gnu_old_sparse(istream_t *fp, tar_header_t *hdr);

sparse_map_t *read_gnu_new_sparse(istream_t *fp, tar_header_decoded_t *out);

void free_sparse_list(sparse_map_t *sparse);

size_t base64_decode(sqfs_u8 *out, const char *in, size_t len);

void urldecode(char *str);

char *record_to_memory(istream_t *fp, size_t size);

int read_pax_header(istream_t *fp, sqfs_u64 entsize, unsigned int *set_by_pax,
		    tar_header_decoded_t *out);

#endif /* INTERNAL_H */
