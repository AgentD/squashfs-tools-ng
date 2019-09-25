/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * read_sparse_map.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "internal.h"

sparse_map_t *read_sparse_map(const char *line)
{
	sparse_map_t *last = NULL, *list = NULL, *ent = NULL;

	do {
		ent = calloc(1, sizeof(*ent));
		if (ent == NULL)
			goto fail_errno;

		if (pax_read_decimal(line, &ent->offset))
			goto fail_format;

		while (isdigit(*line))
			++line;

		if (*(line++) != ',')
			goto fail_format;

		if (pax_read_decimal(line, &ent->count))
			goto fail_format;

		while (isdigit(*line))
			++line;

		if (last == NULL) {
			list = last = ent;
		} else {
			last->next = ent;
			last = ent;
		}
	} while (*(line++) == ',');

	return list;
fail_errno:
	perror("parsing GNU pax sparse file record");
	goto fail;
fail_format:
	fputs("malformed GNU pax sparse file record\n", stderr);
	goto fail;
fail:
	free_sparse_list(list);
	free(ent);
	return NULL;
}
