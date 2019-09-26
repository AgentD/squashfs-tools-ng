/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * mkfs.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef MKFS_H
#define MKFS_H

#include "config.h"

#include "sqfs/meta_writer.h"
#include "sqfs/compress.h"
#include "sqfs/id_table.h"
#include "sqfs/data.h"
#include "sqfs/io.h"

#include "highlevel.h"
#include "fstree.h"
#include "util.h"

#include <getopt.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
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

void process_command_line(options_t *opt, int argc, char **argv);

#endif /* MKFS_H */
