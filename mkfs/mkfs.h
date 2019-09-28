/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * mkfs.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef MKFS_H
#define MKFS_H

#include "config.h"

#include "sqfs/xattr_writer.h"
#include "sqfs/meta_writer.h"
#include "sqfs/compress.h"
#include "sqfs/id_table.h"
#include "sqfs/block.h"
#include "sqfs/io.h"

#include "highlevel.h"
#include "fstree.h"
#include "util.h"

#ifdef HAVE_SYS_XATTR_H
#include <sys/xattr.h>
#endif

#ifdef WITH_SELINUX
#include <selinux/selinux.h>
#include <selinux/label.h>
#endif

#include <getopt.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <dirent.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>

typedef struct {
	E_SQFS_COMPRESSOR compressor;
	char *fs_defaults;
	int outmode;
	int blksz;
	int devblksz;
	unsigned int dirscan_flags;
	unsigned int num_jobs;
	size_t max_backlog;
	bool exportable;
	bool quiet;
	const char *infile;
	const char *packdir;
	const char *outfile;
	const char *selinux;
	char *comp_extra;
} options_t;

enum {
	DIR_SCAN_KEEP_TIME = 0x01,

	DIR_SCAN_ONE_FILESYSTEM = 0x02,

	DIR_SCAN_READ_XATTR = 0x04,
};

void process_command_line(options_t *opt, int argc, char **argv);

int fstree_from_dir(fstree_t *fs, const char *path, void *selinux_handle,
		    sqfs_xattr_writer_t *xwr, unsigned int flags);


void *selinux_open_context_file(const char *filename);

int selinux_relable_node(void *sehnd, sqfs_xattr_writer_t *xwr,
			 tree_node_t *node, const char *path);

void selinux_close_context_file(void *sehnd);

#endif /* MKFS_H */
