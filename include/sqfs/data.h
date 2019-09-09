/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * data.h - This file is part of libsquashfs
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#ifndef SQFS_DATA_H
#define SQFS_DATA_H

#include "sqfs/predef.h"

#define SQFS_META_BLOCK_SIZE 8192

#define SQFS_IS_BLOCK_COMPRESSED(size) (((size) & (1 << 24)) == 0)
#define SQFS_ON_DISK_BLOCK_SIZE(size) ((size) & ((1 << 24) - 1))
#define SQFS_IS_SPARSE_BLOCK(size) (SQFS_ON_DISK_BLOCK_SIZE(size) == 0)

struct sqfs_fragment_t {
	uint64_t start_offset;
	uint32_t size;
	uint32_t pad0;
};

#endif /* SQFS_DATA_H */
