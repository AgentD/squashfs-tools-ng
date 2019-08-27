/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * alloc.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "util.h"

#include <stddef.h>
#include <stdlib.h>
#include <errno.h>

void *alloc_flex(size_t base_size, size_t item_size, size_t nmemb)
{
	size_t size;

	if (SZ_MUL_OV(nmemb, item_size, &size) ||
	    SZ_ADD_OV(base_size, size, &size)) {
		errno = EOVERFLOW;
		return NULL;
	}

	return calloc(1, size);
}

void *alloc_array(size_t item_size, size_t nmemb)
{
	size_t size;

	if (SZ_MUL_OV(nmemb, item_size, &size)) {
		errno = EOVERFLOW;
		return NULL;
	}

	return calloc(1, size);
}

void *alloc_string(size_t len)
{
	size_t size;

	if (SZ_ADD_OV(len, 1, &size)) {
		errno = EOVERFLOW;
		return NULL;
	}

	return calloc(1, len);
}
