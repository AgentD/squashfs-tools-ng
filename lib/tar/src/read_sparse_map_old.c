/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * read_sparse_map_old.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "internal.h"

#include <ctype.h>
#include <stdlib.h>

static int parse(const gnu_old_sparse_t *in, size_t count,
		 sparse_map_t **head, sparse_map_t **tail)
{
	sparse_map_t *node;
	sqfs_u64 off, sz;

	while (count--) {
		if (!isdigit(in->offset[0]) || !isdigit(in->numbytes[0]))
			return 1;
		if (read_number(in->offset, sizeof(in->offset), &off))
			return -1;
		if (read_number(in->numbytes, sizeof(in->numbytes), &sz))
			return -1;
		++in;

		node = calloc(1, sizeof(*node));
		if (node == NULL) {
			perror("parsing GNU sparse header");
			return -1;
		}

		node->offset = off;
		node->count = sz;

		if ((*head) == NULL) {
			(*head) = (*tail) = node;
		} else {
			(*tail)->next = node;
			(*tail) = node;
		}
	}

	return 0;
}

sparse_map_t *read_gnu_old_sparse(sqfs_istream_t *fp, tar_header_t *hdr)
{
	sparse_map_t *list = NULL, *end = NULL;
	gnu_old_sparse_record_t sph;
	int ret;

	ret = parse(hdr->tail.gnu.sparse, 4, &list, &end);
	if (ret < 0)
		goto fail;

	if (ret > 0 || hdr->tail.gnu.isextended == 0)
		return list;

	do {
		ret = sqfs_istream_read(fp, &sph, sizeof(sph));
		if (ret < 0)
			goto fail;

		if ((size_t)ret < sizeof(sph))
			goto fail_eof;

		ret = parse(sph.sparse, 21, &list, &end);
		if (ret < 0)
			goto fail;
	} while (ret == 0 && sph.isextended != 0);

	return list;
fail_eof:
	fputs("reading GNU sparse header: unexpected end-of-file\n", stderr);
fail:
	free_sparse_list(list);
	return NULL;
}
