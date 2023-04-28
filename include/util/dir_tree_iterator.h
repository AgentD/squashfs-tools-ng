/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * dir_tree_iterator.h
 *
 * Copyright (C) 2023 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef UTIL_DIR_TREE_ITERATOR_H
#define UTIL_DIR_TREE_ITERATOR_H

#include "util/dir_iterator.h"

enum {
	DIR_SCAN_NO_SOCK = 0x0001,
	DIR_SCAN_NO_SLINK = 0x0002,
	DIR_SCAN_NO_FILE = 0x0004,
	DIR_SCAN_NO_BLK = 0x0008,
	DIR_SCAN_NO_DIR = 0x0010,
	DIR_SCAN_NO_CHR = 0x0020,
	DIR_SCAN_NO_FIFO = 0x0040,

	DIR_SCAN_KEEP_TIME = 0x0100,
	DIR_SCAN_ONE_FILESYSTEM = 0x0200,
	DIR_SCAN_NO_RECURSION = 0x0400,
};

typedef struct {
	sqfs_u32 flags;
	sqfs_s64 def_mtime;
} dir_tree_cfg_t;

dir_iterator_t *dir_tree_iterator_create(const char *path,
					 const dir_tree_cfg_t *cfg);

void dir_tree_iterator_skip(dir_iterator_t *it);

#endif /* UTIL_DIR_TREE_ITERATOR_H */
