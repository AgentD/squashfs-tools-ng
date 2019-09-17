/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * rdsquashfs.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef RDSQUASHFS_H
#define RDSQUASHFS_H

#include "config.h"

#include "sqfs/meta_reader.h"
#include "sqfs/compress.h"
#include "sqfs/id_table.h"
#include "sqfs/data.h"
#include "data_reader.h"
#include "highlevel.h"
#include "fstree.h"
#include "util.h"

#include <sys/sysmacros.h>
#include <sys/types.h>
#ifdef HAVE_SYS_XATTR_H
#include <sys/xattr.h>
#endif
#include <sys/prctl.h>
#include <sys/wait.h>
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

void list_files(tree_node_t *node);

int restore_fstree(tree_node_t *root, int flags);

int update_tree_attribs(fstree_t *fs, tree_node_t *root, int flags);

int fill_unpacked_files(fstree_t *fs, data_reader_t *data, int flags);

int describe_tree(tree_node_t *root, const char *unpack_root);

void process_command_line(options_t *opt, int argc, char **argv);

#endif /* RDSQUASHFS_H */
