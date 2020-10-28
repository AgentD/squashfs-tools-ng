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
#include "tar.h"

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

void process_args(int argc, char **argv);

/* process_tarball.c */
int process_tarball(istream_t *input_file, sqfs_writer_t *sqfs);

#endif /* TAR2SQFS_H */
