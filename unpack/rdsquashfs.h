/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RDSQUASHFS_H
#define RDSQUASHFS_H

#include "meta_reader.h"
#include "frag_reader.h"
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

typedef struct {
	compressor_t *cmp;
	size_t block_size;
	frag_reader_t *frag;
	int rdtree_flags;
	int sqfsfd;
	int flags;

	void *buffer;
	void *scratch;
} unsqfs_info_t;

void list_files(tree_node_t *node);

int extract_file(file_info_t *fi, unsqfs_info_t *info, int outfd);

int restore_fstree(const char *rootdir, tree_node_t *root,
		   unsqfs_info_t *info);

void describe_tree(tree_node_t *root, const char *unpack_root);

#endif /* RDSQUASHFS_H */
