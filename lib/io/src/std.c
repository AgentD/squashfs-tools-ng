/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * std.c
 *
 * Copyright (C) 2023 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "compat.h"
#include "io/std.h"
#include "sqfs/io.h"

#if defined(_WIN32) || defined(__WINDOWS__)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#define STD_INPUT_HANDLE STDIN_FILENO
#define STD_OUTPUT_HANDLE STDOUT_FILENO
#define GetStdHandle(hnd) hnd
#endif

int istream_open_stdin(sqfs_istream_t **out)
{
	sqfs_file_handle_t hnd = GetStdHandle(STD_INPUT_HANDLE);

	return sqfs_istream_open_handle(out, "stdin", hnd, 0);
}

int ostream_open_stdout(sqfs_ostream_t **out)
{
	sqfs_file_handle_t hnd = GetStdHandle(STD_OUTPUT_HANDLE);

	return sqfs_ostream_open_handle(out, "stdout", hnd,
					SQFS_FILE_OPEN_NO_SPARSE);
}
