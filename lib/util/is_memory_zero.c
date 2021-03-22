/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * is_memory_zero.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "util.h"

#include <stdint.h>

#define U64THRESHOLD (128)

static bool test_u8(const unsigned char *blob, size_t size)
{
	while (size--) {
		if (*(blob++) != 0)
			return false;
	}

	return true;
}

bool is_memory_zero(const void *blob, size_t size)
{
	const sqfs_u64 *u64ptr;
	size_t diff;

	if (size < U64THRESHOLD)
		return test_u8(blob, size);

	diff = (uintptr_t)blob % sizeof(sqfs_u64);

	if (diff != 0) {
		diff = sizeof(sqfs_u64) - diff;

		if (!test_u8(blob, diff))
			return false;

		blob = (const char *)blob + diff;
		size -= diff;
	}

	u64ptr = blob;

	while (size >= sizeof(sqfs_u64)) {
		if (*(u64ptr++) != 0)
			return false;

		size -= sizeof(sqfs_u64);
	}

	return test_u8((const unsigned char *)u64ptr, size);
}
