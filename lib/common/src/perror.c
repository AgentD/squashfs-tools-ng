/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * print_version.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "common.h"

#include <stdio.h>

#if defined(_WIN32) || defined(__WINDOWS__)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <errno.h>
#include <string.h>
#endif

void sqfs_perror(const char *file, const char *action, int error_code)
{
	const char *errstr;
#if defined(_WIN32) || defined(__WINDOWS__)
	DWORD syserror = GetLastError();
#else
	int syserror = errno;
#endif

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
	case SQFS_ERROR_SEQUENCE:
		errstr = "illegal oder of operations";
		break;
	default:
		errstr = "libsquashfs returned an unknown error code";
		break;
	}

	if (file != NULL)
		fprintf(stderr, "%s: ", file);

	if (action != NULL)
		fprintf(stderr, "%s: ", action);

	fprintf(stderr, "%s.\n", errstr);

	if (error_code == SQFS_ERROR_IO) {
#if defined(_WIN32) || defined(__WINDOWS__)
		LPVOID msg = NULL;

		FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER |
			       FORMAT_MESSAGE_FROM_SYSTEM |
			       FORMAT_MESSAGE_IGNORE_INSERTS,
			       NULL, syserror,
			       MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			       (LPTSTR)&msg, 0, NULL);

		fprintf(stderr, "OS error: %s\n", (const char *)msg);

		if (msg != NULL)
			LocalFree(msg);
#else
		fprintf(stderr, "OS error: %s\n", strerror(syserror));
#endif
	}
}
