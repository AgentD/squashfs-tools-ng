/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "internal.h"

sparse_map_t *read_gnu_old_sparse(int fd, tar_header_t *hdr)
{
	sparse_map_t *list = NULL, *end = NULL, *node;
	gnu_sparse_t sph;
	uint64_t off, sz;
	ssize_t ret;
	int i;

	for (i = 0; i < 4; ++i) {
		if (!isdigit(hdr->tail.gnu.sparse[i].offset[0]))
			break;
		if (!isdigit(hdr->tail.gnu.sparse[i].numbytes[0]))
			break;

		if (read_octal(hdr->tail.gnu.sparse[i].offset,
			       sizeof(hdr->tail.gnu.sparse[i].offset), &off))
			goto fail;
		if (read_octal(hdr->tail.gnu.sparse[i].numbytes,
			       sizeof(hdr->tail.gnu.sparse[i].numbytes), &sz))
			goto fail;

		node = calloc(1, sizeof(*node));
		if (node == NULL)
			goto fail_errno;

		node->offset = off;
		node->count = sz;

		if (list == NULL) {
			list = end = node;
		} else {
			end->next = node;
			end = node;
		}
	}

	if (hdr->tail.gnu.isextended == 0)
		return list;

	do {
		ret = read_retry(fd, &sph, sizeof(sph));
		if (ret < 0)
			goto fail_errno;
		if ((size_t)ret < sizeof(sph))
			goto fail_eof;

		for (i = 0; i < 21; ++i) {
			if (!isdigit(sph.sparse[i].offset[0]))
				break;
			if (!isdigit(sph.sparse[i].numbytes[0]))
				break;

			if (read_octal(sph.sparse[i].offset,
				       sizeof(sph.sparse[i].offset), &off))
				goto fail;
			if (read_octal(sph.sparse[i].numbytes,
				       sizeof(sph.sparse[i].numbytes), &sz))
				goto fail;

			node = calloc(1, sizeof(*node));
			if (node == NULL)
				goto fail_errno;

			node->offset = off;
			node->count = sz;

			if (list == NULL) {
				list = end = node;
			} else {
				end->next = node;
				end = node;
			}
		}
	} while (sph.isextended != 0);

	return list;
fail_eof:
	fputs("parsing GNU sparse header: unexpected end of file", stderr);
	goto fail;
fail_errno:
	perror("parsing GNU sparse header");
	goto fail;
fail:
	free_sparse_list(list);
	return NULL;
}
