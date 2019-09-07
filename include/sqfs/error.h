/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * error.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef SQFS_ERROR_H
#define SQFS_ERROR_H

typedef enum {
	SQFS_ERROR_ALLOC = -1,
	SQFS_ERROR_IO = -2,
	SQFS_ERROR_COMRPESSOR = -3,
	SQFS_ERROR_INTERNAL = -4,
	SQFS_ERROR_CORRUPTED = -5,
	SQFS_ERROR_UNSUPPORTED = -6,
	SQFS_ERROR_OVERFLOW = -7,
	SQFS_ERROR_OUT_OF_BOUNDS = -8,

	SFQS_ERROR_SUPER_MAGIC = -9,
	SFQS_ERROR_SUPER_VERSION = -10,
	SQFS_ERROR_SUPER_BLOCK_SIZE = -11,
} E_SQFS_ERROR;

#endif /* SQFS_ERROR_H */
