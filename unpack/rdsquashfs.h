/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * rdsquashfs.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef RDSQUASHFS_H
#define RDSQUASHFS_H

#include "config.h"

#include "sqfs/xattr_reader.h"
#include "sqfs/meta_reader.h"
#include "sqfs/data_reader.h"
#include "sqfs/compressor.h"
#include "sqfs/id_table.h"
#include "sqfs/xattr.h"
#include "sqfs/block.h"

#include "highlevel.h"
#include "fstree.h"
#include "util.h"

#ifdef HAVE_SYS_XATTR_H
#include <sys/xattr.h>
#endif
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>

enum UNPACK_FLAGS {
	UNPACK_CHMOD = 0x01,
	UNPACK_CHOWN = 0x02,
	UNPACK_QUIET = 0x04,
	UNPACK_NO_SPARSE = 0x08,
	UNPACK_SET_XATTR = 0x10,
	UNPACK_SET_TIMES = 0x20,
};

enum {
	OP_NONE = 0,
	OP_LS,
	OP_CAT,
	OP_UNPACK,
	OP_DESCRIBE,
	OP_RDATTR,
};

typedef struct {
	int op;
	int rdtree_flags;
	int flags;
	char *cmdpath;
	const char *unpack_root;
	const char *image_name;
} options_t;

void list_files(const sqfs_tree_node_t *node);

int restore_fstree(sqfs_tree_node_t *root, int flags);

int update_tree_attribs(sqfs_xattr_reader_t *xattr,
			const sqfs_tree_node_t *root, int flags);

int fill_unpacked_files(size_t blk_sz, const sqfs_tree_node_t *root,
			sqfs_data_reader_t *data, int flags);

int describe_tree(const sqfs_tree_node_t *root, const char *unpack_root);

int dump_xattrs(sqfs_xattr_reader_t *xattr, const sqfs_inode_generic_t *inode);

void process_command_line(options_t *opt, int argc, char **argv);

#endif /* RDSQUASHFS_H */
