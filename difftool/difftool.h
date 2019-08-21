/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * difftool.h
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

extern const char *first_path;
extern const char *second_path;
extern int compare_flags;
extern sqfs_reader_t sqfs_a;
extern sqfs_reader_t sqfs_b;
extern bool a_is_dir;
extern bool b_is_dir;

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

int compare_files(file_info_t *a, file_info_t *b, const char *path);

int node_compare(tree_node_t *a, tree_node_t *b);

int compare_super_blocks(const sqfs_super_t *a, const sqfs_super_t *b);

int extract_files(file_info_t *a, file_info_t *b, const char *path);

#endif /* DIFFTOOL_H */
