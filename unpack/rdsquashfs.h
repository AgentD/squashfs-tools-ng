/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RDSQUASHFS_H
#define RDSQUASHFS_H

#include "meta_reader.h"
#include "data_reader.h"
#include "highlevel.h"
#include "squashfs.h"
#include "compress.h"
#include "id_table.h"
#include "fstree.h"
#include "config.h"
#include "util.h"

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
};

void list_files(tree_node_t *node);

int restore_fstree(const char *rootdir, tree_node_t *root,
		   data_reader_t *data, int flags);

void describe_tree(tree_node_t *root, const char *unpack_root);

#endif /* RDSQUASHFS_H */
