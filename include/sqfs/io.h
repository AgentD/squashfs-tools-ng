/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * io.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef SQFS_IO_H
#define SQFS_IO_H

#include "sqfs/predef.h"

typedef enum {
	SQFS_FILE_OPEN_READ_ONLY = 0x01,

	SQFS_FILE_OPEN_OVERWRITE = 0x02,

	SQFS_FILE_OPEN_ALL_FLAGS = 0x03,
} E_SQFS_FILE_OPEN_FLAGS;

struct sqfs_file_t {
	void (*destroy)(sqfs_file_t *file);

	int (*read_at)(sqfs_file_t *file, uint64_t offset,
		       void *buffer, size_t size);

	int (*write_at)(sqfs_file_t *file, uint64_t offset,
			const void *buffer, size_t size);

	uint64_t (*get_size)(sqfs_file_t *file);

	int (*truncate)(sqfs_file_t *file, uint64_t size);
};

#ifdef __cplusplus
extern "C" {
#endif

SQFS_API sqfs_file_t *sqfs_open_file(const char *filename, int flags);

#ifdef __cplusplus
}
#endif

#endif /* SQFS_IO_H */
