/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RDSQUASHFS_H
#define RDSQUASHFS_H

#include "config.h"

#include "meta_reader.h"
#include "data_reader.h"
#include "highlevel.h"
#include "squashfs.h"
#include "compress.h"
#include "id_table.h"
#include "fstree.h"
#include "util.h"

#include <sys/sysmacros.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>

enum UNPACK_FLAGS {
	UNPACK_CHMOD = 0x01,
	UNPACK_CHOWN = 0x02,
	UNPACK_QUIET = 0x04,
	UNPACK_NO_SPARSE = 0x08,
};

enum {
	OP_NONE = 0,
	OP_LS,
	OP_CAT,
	OP_UNPACK,
	OP_DESCRIBE,
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

int update_tree_attribs(tree_node_t *root, int flags);

int fill_unpacked_files(fstree_t *fs, data_reader_t *data, int flags);

void describe_tree(tree_node_t *root, const char *unpack_root);

void process_command_line(options_t *opt, int argc, char **argv);

#endif /* RDSQUASHFS_H */
