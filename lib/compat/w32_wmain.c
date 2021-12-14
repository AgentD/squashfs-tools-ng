/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * w32_wmain.c
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
#include <shellapi.h>

#undef main

int main(int argc, char **argv)
{
	WCHAR *cmdline, **argList;
	int i, ret, utf8_argc;
	char **utf8_argv;
	(void)argc;
	(void)argv;

	/* get the UTF-16 encoded command line arguments */
	cmdline = GetCommandLineW();
	argList = CommandLineToArgvW(cmdline, &utf8_argc);
	if (argList == NULL)
		goto fail_oom;

	/* convert to UTF-8 */
	utf8_argv = calloc(sizeof(utf8_argv[0]), utf8_argc);
	if (utf8_argv == NULL)
		goto fail_oom;

	for (i = 0; i < utf8_argc; ++i) {
		DWORD length = WideCharToMultiByte(CP_UTF8, 0, argList[i],
						   -1, NULL, 0, NULL, NULL);
		if (length <= 0)
			goto fail_conv;

		utf8_argv[i] = calloc(1, length + 1);
		if (utf8_argv[i] == NULL)
			goto fail_oom;

		WideCharToMultiByte(CP_UTF8, 0, argList[i], -1,
				    utf8_argv[i], length + 1, NULL, NULL);
		utf8_argv[i][length] = '\0';
	}

	LocalFree(argList);
	argList = NULL;

	/* call the actual main function */
	ret = sqfs_tools_main(utf8_argc, utf8_argv);

	/* cleanup */
	for (i = 0; i < utf8_argc; ++i)
		free(utf8_argv[i]);

	free(utf8_argv);
	return ret;
fail_conv:
	w32_perror("Converting UTF-16 argument to UTF-8");
	goto fail;
fail_oom:
	fputs("out of memory\n", stderr);
	goto fail;
fail:
	if (utf8_argv != NULL) {
		for (i = 0; i < utf8_argc; ++i)
			free(utf8_argv[i]);
		free(utf8_argv);
	}
	if (argList != NULL) {
		LocalFree(argList);
	}
	return EXIT_FAILURE;
}
#endif
