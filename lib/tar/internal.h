/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef INTERNAL_H
#define INTERNAL_H

#include "util.h"
#include "tar.h"

#include <sys/sysmacros.h>
#include <string.h>
#include <stdlib.h>
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
	PAX_ATIME = 0x080,
	PAX_MTIME = 0x100,
	PAX_CTIME = 0x200,
	PAX_SPARSE_SIZE = 0x400,
};

enum {
	ETV_UNKNOWN = 0,
	ETV_V7_UNIX,
	ETV_PRE_POSIX,
	ETV_POSIX,
};

int read_octal(const char *str, int digits, uint64_t *out);

int read_binary(const char *str, int digits, uint64_t *out);

int read_number(const char *str, int digits, uint64_t *out);

int pax_read_decimal(const char *str, uint64_t *out);

void update_checksum(tar_header_t *hdr);

bool is_checksum_valid(const tar_header_t *hdr);

sparse_map_t *read_sparse_map(const char *line);

sparse_map_t *read_gnu_old_sparse(int fd, tar_header_t *hdr);

void free_sparse_list(sparse_map_t *sparse);

#endif /* INTERNAL_H */
