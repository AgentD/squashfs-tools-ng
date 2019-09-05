/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * readdir.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "sqfs/dir.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int sqfs_meta_reader_read_dir_header(sqfs_meta_reader_t *m,
				     sqfs_dir_header_t *hdr)
{
	if (sqfs_meta_reader_read(m, hdr, sizeof(*hdr)))
		return -1;

	hdr->count = le32toh(hdr->count);
	hdr->start_block = le32toh(hdr->start_block);
	hdr->inode_number = le32toh(hdr->inode_number);

	if (hdr->count > (SQFS_MAX_DIR_ENT - 1)) {
		fputs("Found a directory header with too many entries\n",
		      stderr);
		return -1;
	}

	return 0;
}

sqfs_dir_entry_t *sqfs_meta_reader_read_dir_ent(sqfs_meta_reader_t *m)
{
	sqfs_dir_entry_t ent, *out;
	uint16_t *diff_u16;

	if (sqfs_meta_reader_read(m, &ent, sizeof(ent)))
		return NULL;

	diff_u16 = (uint16_t *)&ent.inode_diff;
	*diff_u16 = le16toh(*diff_u16);

	ent.offset = le16toh(ent.offset);
	ent.type = le16toh(ent.type);
	ent.size = le16toh(ent.size);

	out = calloc(1, sizeof(*out) + ent.size + 2);
	if (out == NULL) {
		perror("reading dir entry");
		return NULL;
	}

	*out = ent;
	if (sqfs_meta_reader_read(m, out->name, ent.size + 1)) {
		free(out);
		return NULL;
	}

	if (strchr((char *)out->name, '/') != NULL ||
	    strchr((char *)out->name, '\\') != NULL) {
		fputs("Found a file name that contains slashes\n", stderr);
		free(out);
		return NULL;
	}

	if (strcmp((char *)out->name, "..") == 0 ||
	    strcmp((char *)out->name, ".") == 0) {
		fputs("Found '..' or '.' in file names\n", stderr);
		free(out);
		return NULL;
	}

	return out;
}
