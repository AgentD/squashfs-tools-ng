/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * cleanup.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "internal.h"

void free_sparse_list(sparse_map_t *sparse)
{
	sparse_map_t *old;

	while (sparse != NULL) {
		old = sparse;
		sparse = sparse->next;
		free(old);
	}
}

void free_xattr_list(tar_xattr_t *list)
{
	tar_xattr_t *old;

	while (list != NULL) {
		old = list;
		list = list->next;
		free(old);
	}
}

void clear_header(tar_header_decoded_t *hdr)
{
	free_xattr_list(hdr->xattr);
	free_sparse_list(hdr->sparse);
	free(hdr->name);
	free(hdr->link_target);
	memset(hdr, 0, sizeof(*hdr));
}
