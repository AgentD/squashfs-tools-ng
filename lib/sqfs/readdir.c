/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "meta_reader.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int meta_reader_read_dir_header(meta_reader_t *m, sqfs_dir_header_t *hdr)
{
	if (meta_reader_read(m, hdr, sizeof(*hdr)))
		return -1;

	hdr->count = le32toh(hdr->count);
	hdr->start_block = le32toh(hdr->start_block);
	hdr->inode_number = le32toh(hdr->inode_number);
	return 0;
}

sqfs_dir_entry_t *meta_reader_read_dir_ent(meta_reader_t *m)
{
	sqfs_dir_entry_t ent, *out;

	if (meta_reader_read(m, &ent, sizeof(ent)))
		return NULL;

	ent.offset = le16toh(ent.offset);
	ent.inode_number = le16toh(ent.inode_number);
	ent.type = le16toh(ent.type);
	ent.size = le16toh(ent.size);

	out = calloc(1, sizeof(*out) + ent.size + 2);
	if (out == NULL) {
		perror("reading dir entry");
		return NULL;
	}

	*out = ent;
	if (meta_reader_read(m, out->name, ent.size + 1)) {
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
