/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef MKSQUASHFS_H
#define MKSQUASHFS_H

#include "squashfs.h"
#include "compress.h"
#include "fstree.h"
#include "config.h"

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>

typedef struct {
	unsigned int def_uid;
	unsigned int def_gid;
	unsigned int def_mode;
	unsigned int def_mtime;
	int outmode;
	int compressor;
	int blksz;
	int devblksz;
	const char *infile;
	const char *outfile;
} options_t;

typedef struct {
	uint8_t data[SQFS_META_BLOCK_SIZE + 2];
	size_t offset;
	size_t block_offset;
	int outfd;
	compressor_t *cmp;
} meta_writer_t;

void process_command_line(options_t *opt, int argc, char **argv);

int sqfs_super_init(sqfs_super_t *s, const options_t *opt);

int sqfs_padd_file(sqfs_super_t *s, const options_t *opt, int outfd);

int sqfs_super_write(const sqfs_super_t *super, int outfd);

meta_writer_t *meta_writer_create(int fd, compressor_t *cmp);

void meta_writer_destroy(meta_writer_t *m);

int meta_writer_flush(meta_writer_t *m);

int meta_writer_append(meta_writer_t *m, const void *data, size_t size);

#endif /* MKSQUASHFS_H */
