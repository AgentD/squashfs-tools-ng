/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef MKFS_H
#define MKFS_H

#include "meta_writer.h"
#include "data_writer.h"
#include "highlevel.h"
#include "squashfs.h"
#include "compress.h"
#include "id_table.h"
#include "fstree.h"
#include "config.h"
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
	bool keep_time;
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
