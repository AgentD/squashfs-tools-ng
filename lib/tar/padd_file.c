/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * padd_file.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "tar/tar.h"
#include "tar/format.h"

int padd_file(ostream_t *fp, sqfs_u64 size)
{
	size_t padd_sz = size % TAR_RECORD_SIZE;

	if (padd_sz == 0)
		return 0;

	return ostream_append_sparse(fp, TAR_RECORD_SIZE - padd_sz);
}
