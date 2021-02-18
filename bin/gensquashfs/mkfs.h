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

#ifdef HAVE_SYS_XATTR_H
#include <sys/xattr.h>

#if defined(__APPLE__) && defined(__MACH__)
#define llistxattr(path, list, size) \
	listxattr(path, list, size, XATTR_NOFOLLOW)

#define lgetxattr(path, name, value, size) \
	getxattr(path, name, value, size, 0, XATTR_NOFOLLOW)
#endif
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
#include <stdio.h>
#include <errno.h>
#include <ctype.h>

typedef struct {
	sqfs_writer_cfg_t cfg;
	unsigned int dirscan_flags;
	const char *infile;
	const char *selinux;
	bool no_tail_packing;

	/* copied from command line or constructed from infile argument
	   if not specified. Must be free'd. */
	char *packdir;

	unsigned int force_uid_value;
	unsigned int force_gid_value;
	bool force_uid;
	bool force_gid;

	bool scan_xattr;
} options_t;

void process_command_line(options_t *opt, int argc, char **argv);

int xattrs_from_dir(fstree_t *fs, const char *path, void *selinux_handle,
		    sqfs_xattr_writer_t *xwr, bool scan_xattr);

void *selinux_open_context_file(const char *filename);

int selinux_relable_node(void *sehnd, sqfs_xattr_writer_t *xwr,
			 tree_node_t *node, const char *path);

void selinux_close_context_file(void *sehnd);

#endif /* MKFS_H */
