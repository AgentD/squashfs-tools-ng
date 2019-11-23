/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * mkfs.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef MKFS_H
#define MKFS_H

#include "config.h"

#include "common.h"
#include "fstree.h"
#include "util/util.h"

#ifdef HAVE_SYS_XATTR_H
#include <sys/xattr.h>
#endif

#ifdef WITH_SELINUX
#include <selinux/selinux.h>
#include <selinux/label.h>
#endif

#include <getopt.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <dirent.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>

typedef struct {
	sqfs_writer_cfg_t cfg;
	unsigned int dirscan_flags;
	const char *infile;
	const char *packdir;
	const char *selinux;
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
