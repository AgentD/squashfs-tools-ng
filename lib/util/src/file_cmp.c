/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * file_cmp.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "util/util.h"
#include "sqfs/io.h"

#include <string.h>

int check_file_range_equal(sqfs_file_t *file, void *scratch, size_t scratch_sz,
			   sqfs_u64 loc_a, sqfs_u64 loc_b, sqfs_u64 size)
{
	sqfs_u8 *ptr_a = scratch, *ptr_b = ptr_a + scratch_sz / 2;
	int ret;

	while (size > 0) {
		size_t diff = scratch_sz / 2;
		diff = (sqfs_u64)diff > size ? size : diff;

		ret = file->read_at(file, loc_a, ptr_a, diff);
		if (ret != 0)
			return ret;

		ret = file->read_at(file, loc_b, ptr_b, diff);
		if (ret != 0)
			return ret;

		if (memcmp(ptr_a, ptr_b, diff) != 0)
			return 1;

		size -= diff;
		loc_a += diff;
		loc_b += diff;
	}

	return 0;
}
