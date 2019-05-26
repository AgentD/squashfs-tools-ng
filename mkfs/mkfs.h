/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef MKFS_H
#define MKFS_H

#include "squashfs.h"
#include "compress.h"
#include "id_table.h"
#include "fstree.h"
#include "config.h"
#include "table.h"

#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>

typedef enum {
	PACK_NONE,
	PACK_FILE,
	PACK_DIR,
} E_PACK_MODE;

typedef struct {
	unsigned int def_uid;
	unsigned int def_gid;
	unsigned int def_mode;
	unsigned int def_mtime;
	int outmode;
	int compressor;
	int blksz;
	int devblksz;
	bool quiet;
	const char *infile;
	const char *outfile;
	const char *selinux;
	char *comp_extra;
	E_PACK_MODE mode;
} options_t;

typedef struct {
	int outfd;
	options_t opt;
	sqfs_super_t super;
	fstree_t fs;
	void *block;
	void *fragment;
	void *scratch;

	sqfs_fragment_t *fragments;
	size_t num_fragments;
	size_t max_fragments;

	int file_block_count;
	file_info_t *frag_list;
	size_t frag_offset;

	id_table_t idtbl;
	size_t inode_counter;

	compressor_t *cmp;
} sqfs_info_t;

void process_command_line(options_t *opt, int argc, char **argv);

int write_data_to_image(sqfs_info_t *info);

int sqfs_write_inodes(sqfs_info_t *info);

int write_xattr(sqfs_info_t *info);

#endif /* MKFS_H */
