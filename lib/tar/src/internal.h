/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * internal.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef INTERNAL_H
#define INTERNAL_H

#include "config.h"

#include "tar/tar.h"
#include "tar/format.h"
#include "util/util.h"
#include "sqfs/error.h"
#include "common.h"

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

sparse_map_t *read_gnu_old_sparse(sqfs_istream_t *fp, tar_header_t *hdr);

sparse_map_t *read_gnu_new_sparse(sqfs_istream_t *fp,
				  tar_header_decoded_t *out);

char *record_to_memory(sqfs_istream_t *fp, size_t size);

int read_pax_header(sqfs_istream_t *fp, sqfs_u64 entsize,
		    unsigned int *set_by_pax,
		    tar_header_decoded_t *out);

#endif /* INTERNAL_H */
