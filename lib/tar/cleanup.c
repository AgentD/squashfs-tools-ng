/* SPDX-License-Identifier: GPL-3.0-or-later */
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

void clear_header(tar_header_decoded_t *hdr)
{
	free_sparse_list(hdr->sparse);
	free(hdr->name);
	free(hdr->link_target);
	memset(hdr, 0, sizeof(*hdr));
}
