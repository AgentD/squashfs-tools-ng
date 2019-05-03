/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef UNSQUASHFS_H
#define UNSQUASHFS_H

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

tree_node_t *tree_node_from_inode(sqfs_inode_generic_t *inode,
				  const id_table_t *idtbl,
				  const char *name,
				  size_t block_size);

int read_fstree(fstree_t *out, int fd, sqfs_super_t *super, compressor_t *cmp);

void list_files(tree_node_t *node);

int extract_file(file_info_t *fi, compressor_t *cmp, size_t block_size,
		 frag_reader_t *frag, int sqfsfd, int outfd);

#endif /* UNSQUASHFS_H */
