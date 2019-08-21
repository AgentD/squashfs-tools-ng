/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * sqfsdiff.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef DIFFTOOL_H
#define DIFFTOOL_H

#include "config.h"

#include "highlevel.h"
#include "fstree.h"
#include "util.h"

#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#define MAX_WINDOW_SIZE (1024 * 1024 * 4)

typedef struct {
	const char *old_path;
	const char *new_path;
	int compare_flags;
	sqfs_reader_t sqfs_old;
	sqfs_reader_t sqfs_new;
	bool old_is_dir;
	bool new_is_dir;
	bool compare_super;
	const char *extract_dir;

	/* holds the coresponding dirfds if old or new is a directory */
	int old_fd;
	int new_fd;
} sqfsdiff_t;

enum {
	COMPARE_NO_PERM = 0x01,
	COMPARE_NO_OWNER = 0x02,
	COMPARE_NO_CONTENTS = 0x04,
	COMPARE_TIMESTAMP = 0x08,
	COMPARE_INODE_NUM = 0x10,
	COMPARE_EXTRACT_FILES = 0x20,
};

int compare_dir_entries(tree_node_t *a, tree_node_t *b);

char *node_path(tree_node_t *n);

int compare_files(sqfsdiff_t *sd, file_info_t *a, file_info_t *b,
		  const char *path);

int node_compare(sqfsdiff_t *sd, tree_node_t *a, tree_node_t *b);

int compare_super_blocks(const sqfs_super_t *a, const sqfs_super_t *b);

int extract_files(sqfsdiff_t *sd, file_info_t *old, file_info_t *new,
		  const char *path);

void process_options(sqfsdiff_t *sd, int argc, char **argv);

#endif /* DIFFTOOL_H */
