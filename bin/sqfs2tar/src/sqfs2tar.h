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

#include "util/util.h"
#include "util/strlist.h"
#include "tar/tar.h"
#include "xfrm/compress.h"
#include "io/xfrm.h"

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
extern strlist_t subdirs;
extern int compressor;

extern const char *filename;

void process_args(int argc, char **argv);

/* iterator.c */
sqfs_dir_iterator_t *tar_compat_iterator_create(const char *filename);

#endif /* SQFS2TAR_H */
