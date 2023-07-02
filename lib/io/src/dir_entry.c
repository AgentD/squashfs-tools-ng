/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * dir_entry.c
 *
 * Copyright (C) 2023 David Oberhollenzer <goliath@infraroot.at>
 */
#include "io/dir_entry.h"
#include "compat.h"

#include <stdlib.h>
#include <string.h>

sqfs_dir_entry_t *dir_entry_create(const char *name)
{
	size_t len, name_len;
	sqfs_dir_entry_t *out;

	name_len = strlen(name);
	if (SZ_ADD_OV(name_len, 1, &name_len))
		return NULL;
	if (SZ_ADD_OV(sizeof(*out), name_len, &len))
		return NULL;

	out = calloc(1, len);
	if (out == NULL)
		return NULL;

	memcpy(out->name, name, name_len);
	return out;
}
