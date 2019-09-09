/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * error.h - This file is part of libsquashfs
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
#ifndef SQFS_ERROR_H
#define SQFS_ERROR_H

typedef enum {
	SQFS_ERROR_ALLOC = -1,
	SQFS_ERROR_IO = -2,
	SQFS_ERROR_COMRPESSOR = -3,
	SQFS_ERROR_INTERNAL = -4,
	SQFS_ERROR_CORRUPTED = -5,
	SQFS_ERROR_UNSUPPORTED = -6,
	SQFS_ERROR_OVERFLOW = -7,
	SQFS_ERROR_OUT_OF_BOUNDS = -8,

	SFQS_ERROR_SUPER_MAGIC = -9,
	SFQS_ERROR_SUPER_VERSION = -10,
	SQFS_ERROR_SUPER_BLOCK_SIZE = -11,
} E_SQFS_ERROR;

#endif /* SQFS_ERROR_H */
