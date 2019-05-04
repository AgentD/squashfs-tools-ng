/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RDSQUASHFS_H
#define RDSQUASHFS_H

#include "meta_reader.h"
#include "frag_reader.h"
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
	UNPACK_NO_DEVICES = 0x01,
	UNPACK_NO_SOCKETS = 0x02,
	UNPACK_NO_FIFO = 0x04,
	UNPACK_NO_SLINKS = 0x08,
	UNPACK_NO_EMPTY = 0x10,
	UNPACK_CHMOD = 0x20,
	UNPACK_CHOWN = 0x40,
};

tree_node_t *tree_node_from_inode(sqfs_inode_generic_t *inode,
				  const id_table_t *idtbl,
				  const char *name,
				  size_t block_size);

int read_fstree(fstree_t *out, int fd, sqfs_super_t *super, compressor_t *cmp,
		int flags);

void list_files(tree_node_t *node);

int extract_file(file_info_t *fi, compressor_t *cmp, size_t block_size,
		 frag_reader_t *frag, int sqfsfd, int outfd);

int restore_fstree(const char *rootdir, tree_node_t *root, compressor_t *cmp,
		   size_t block_size, frag_reader_t *frag, int sqfsfd,
		   int flags);

#endif /* RDSQUASHFS_H */
