/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * print_version.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "common.h"

#include <stdio.h>

void sqfs_perror(const char *file, const char *action, int error_code)
{
	const char *errstr;

	switch (error_code) {
	case SQFS_ERROR_ALLOC:
		errstr = "out of memory";
		break;
	case SQFS_ERROR_IO:
		errstr = "I/O error";
		break;
	case SQFS_ERROR_COMPRESSOR:
		errstr = "internal compressor error";
		break;
	case SQFS_ERROR_INTERNAL:
		errstr = "internal error";
		break;
	case SQFS_ERROR_CORRUPTED:
		errstr = "data corrupted";
		break;
	case SQFS_ERROR_UNSUPPORTED:
		errstr = "unknown or not supported";
		break;
	case SQFS_ERROR_OVERFLOW:
		errstr = "numeric overflow";
		break;
	case SQFS_ERROR_OUT_OF_BOUNDS:
		errstr = "location out of bounds";
		break;
	case SFQS_ERROR_SUPER_MAGIC:
		errstr = "wrong magic value in super block";
		break;
	case SFQS_ERROR_SUPER_VERSION:
		errstr = "wrong squashfs version in super block";
		break;
	case SQFS_ERROR_SUPER_BLOCK_SIZE:
		errstr = "invalid block size specified in super block";
		break;
	case SQFS_ERROR_NOT_DIR:
		errstr = "target is not a directory";
		break;
	case SQFS_ERROR_NO_ENTRY:
		errstr = "no such file or directory";
		break;
	case SQFS_ERROR_LINK_LOOP:
		errstr = "hard link loop detected";
		break;
	case SQFS_ERROR_NOT_FILE:
		errstr = "target is not a file";
		break;
	case SQFS_ERROR_ARG_INVALID:
		errstr = "invalid argument";
		break;
	default:
		errstr = "libsquashfs returned an unknown error code";
		break;
	}

	fprintf(stderr, "%s: %s: %s.\n", file, action, errstr);
}
