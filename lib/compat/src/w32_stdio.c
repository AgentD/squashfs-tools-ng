/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * w32_stdio.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "compat.h"

#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdarg.h>

#undef fputc
#undef putc
#undef fputs
#undef fprintf
#undef printf

static HANDLE get_handle(FILE *strm)
{
	if (strm != stdout && strm != stderr)
		return INVALID_HANDLE_VALUE;

	return GetStdHandle(strm == stderr ?
			    STD_ERROR_HANDLE : STD_OUTPUT_HANDLE);
}

static BOOL isatty(HANDLE hnd)
{
	if (hnd == INVALID_HANDLE_VALUE)
		return FALSE;

	return (GetFileType(hnd) == FILE_TYPE_CHAR);
}

int sqfs_tools_fputs(const char *str, FILE *strm)
{
	DWORD length;
	WCHAR *wstr;
	HANDLE hnd;
	int ret;

	hnd = get_handle(strm);
	if (!isatty(hnd))
		return fputs(str, strm);

	length = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0);
	if (length <= 0)
		return EOF;

	wstr = calloc(sizeof(wstr[0]), length);
	if (wstr == NULL)
		return EOF;

	MultiByteToWideChar(CP_UTF8, 0, str, -1, wstr, length);

	ret = WriteConsoleW(hnd, wstr, length, NULL, NULL) ? length : EOF;

	free(wstr);
	return ret;
}

int sqfs_tools_fputc(int c, FILE *strm)
{
	char str[2];

	str[0] = c;
	str[1] = '\0';

	return sqfs_tools_fputs(str, strm);
}

static int sqfs_printf_common(FILE *out, const char *fmt, va_list ap)
{
	int ret, len;
	char *str;

	len = _vscprintf(fmt, ap);
	if (len == -1)
		return -1;

	str = malloc((size_t)len + 1);
	if (str == NULL)
		return -1;

	ret = vsprintf(str, fmt, ap);
	if (ret == -1) {
		free(str);
		return -1;
	}

	if (sqfs_tools_fputs(str, out) == EOF)
		ret = -1;

	free(str);
	return ret;
}

int sqfs_tools_printf(const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = sqfs_printf_common(stdout, fmt, ap);
	va_end(ap);
	return ret;
}

int sqfs_tools_fprintf(FILE *strm, const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = sqfs_printf_common(strm, fmt, ap);
	va_end(ap);
	return ret;
}
#endif
