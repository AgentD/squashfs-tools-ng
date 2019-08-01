/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * xattr_reader.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef XATTR_READER_H
#define XATTR_READER_H

#include "config.h"

#include "squashfs.h"
#include "compress.h"
#include "fstree.h"

typedef struct xattr_reader_t xattr_reader_t;

void xattr_reader_destroy(xattr_reader_t *xr);

xattr_reader_t *xattr_reader_create(int sqfsfd, sqfs_super_t *super,
				    compressor_t *cmp);

int xattr_reader_restore_node(xattr_reader_t *xr, fstree_t *fs,
			      tree_node_t *node, uint32_t xattr);

#endif /* XATTR_READER_H */
