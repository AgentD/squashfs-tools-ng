/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * sqfs2tar.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef SQFS2TAR_H
#define SQFS2TAR_H

#include "config.h"
#include "common.h"
#include "tar.h"

#include <getopt.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>

/* options.c */
extern bool dont_skip;
extern bool keep_as_dir;
extern bool no_xattr;
extern bool no_links;

extern char *root_becomes;
extern char **subdirs;
extern size_t num_subdirs;
extern int compressor;

extern const char *filename;

void process_args(int argc, char **argv);

/* tar2sqfs.c */
extern sqfs_xattr_reader_t *xr;
extern sqfs_data_reader_t *data;
extern sqfs_super_t super;
extern ostream_t *out_file;

char *assemble_tar_path(char *name, bool is_dir);

/* xattr.c */
int get_xattrs(const char *name, const sqfs_inode_generic_t *inode,
	       tar_xattr_t **out);

/* write_tree.c */
int write_tree(const sqfs_tree_node_t *n);

#endif /* SQFS2TAR_H */
