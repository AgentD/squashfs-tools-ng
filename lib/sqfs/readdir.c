/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * readdir.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "config.h"

#include "sqfs/meta_reader.h"
#include "sqfs/error.h"
#include "sqfs/dir.h"

#include <stdlib.h>
#include <string.h>

int sqfs_meta_reader_read_dir_header(sqfs_meta_reader_t *m,
				     sqfs_dir_header_t *hdr)
{
	int err = sqfs_meta_reader_read(m, hdr, sizeof(*hdr));
	if (err)
		return err;

	hdr->count = le32toh(hdr->count);
	hdr->start_block = le32toh(hdr->start_block);
	hdr->inode_number = le32toh(hdr->inode_number);

	if (hdr->count > (SQFS_MAX_DIR_ENT - 1))
		return SQFS_ERROR_CORRUPTED;

	return 0;
}

int sqfs_meta_reader_read_dir_ent(sqfs_meta_reader_t *m,
				  sqfs_dir_entry_t **result)
{
	sqfs_dir_entry_t ent, *out;
	uint16_t *diff_u16;
	int err;

	err = sqfs_meta_reader_read(m, &ent, sizeof(ent));
	if (err)
		return err;

	diff_u16 = (uint16_t *)&ent.inode_diff;
	*diff_u16 = le16toh(*diff_u16);

	ent.offset = le16toh(ent.offset);
	ent.type = le16toh(ent.type);
	ent.size = le16toh(ent.size);

	out = calloc(1, sizeof(*out) + ent.size + 2);
	if (out == NULL)
		return SQFS_ERROR_ALLOC;

	*out = ent;
	err = sqfs_meta_reader_read(m, out->name, ent.size + 1);
	if (err) {
		free(out);
		return err;
	}

	*result = out;
	return 0;
}
