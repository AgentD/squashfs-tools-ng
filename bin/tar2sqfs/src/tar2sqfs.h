/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * tar2sqfs.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef TAR2SQFS_H
#define TAR2SQFS_H

#include "config.h"
#include "common.h"
#include "compat.h"

#include "util/util.h"
#include "util/strlist.h"
#include "tar/tar.h"
#include "tar/format.h"
#include "xfrm/compress.h"

#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

/* options.c */
extern bool dont_skip;
extern bool keep_time;
extern bool no_tail_pack;
extern bool no_symlink_retarget;
extern sqfs_writer_cfg_t cfg;
extern char *root_becomes;
extern strlist_t excludedirs;

void process_args(int argc, char **argv);

/* process_tarball.c */
int process_tarball(sqfs_dir_iterator_t *it, sqfs_writer_t *sqfs);

#endif /* TAR2SQFS_H */
