/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * read_sparse_map_new.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "internal.h"

static int decode(const char *str, size_t len, size_t *out)
{
	size_t count = 0;

	*out = 0;

	while (count < len && isdigit(*str)) {
		if (SZ_MUL_OV(*out, 10, out))
			return -1;
		if (SZ_ADD_OV(*out, (*(str++) - '0'), out))
			return -1;
		++count;
	}

	if (count == 0 || count == len)
		return 0;

	return (*str == '\n') ? ((int)count + 1) : -1;
}

sparse_map_t *read_gnu_new_sparse(istream_t *fp, tar_header_decoded_t *out)
{
	sparse_map_t *last = NULL, *list = NULL, *ent = NULL;
	size_t i, count, value;
	char buffer[1024];
	int diff, ret;

	if (out->record_size < 512)
		goto fail_format;

	ret = istream_read(fp, buffer, 512);
	if (ret < 0)
		goto fail;

	if (ret < 512)
		goto fail_format;

	diff = decode(buffer, 512, &count);
	if (diff <= 0)
		goto fail_format;

	out->record_size -= 512;

	if (count == 0 || count > TAR_MAX_SPARSE_ENT)
		goto fail_format;

	for (i = 0; i < (count * 2); ++i) {
		ret = decode(buffer + diff, 512 - diff, &value);
		if (ret < 0)
			goto fail_format;

		if (ret > 0) {
			diff += ret;
		} else {
			if (out->record_size < 512)
				goto fail_format;

			ret = istream_read(fp, buffer + 512, 512);
			if (ret < 0)
				goto fail;

			if (ret < 512)
				goto fail_format;

			ret = decode(buffer + diff, 1024 - diff, &value);
			if (ret <= 0)
				goto fail_format;

			memcpy(buffer, buffer + 512, 512);
			diff = diff + ret - 512;
			out->record_size -= 512;
		}

		if ((i & 0x01) == 0) {
			ent = calloc(1, sizeof(*ent));
			if (ent == NULL)
				goto fail_errno;

			if (list == NULL) {
				list = last = ent;
			} else {
				last->next = ent;
				last = ent;
			}

			ent->offset = value;
		} else {
			ent->count = value;
		}
	}

	return list;
fail_errno:
	perror("parsing GNU 1.0 style sparse file record");
	goto fail;
fail_format:
	fputs("Malformed GNU 1.0 style sparse file map.\n", stderr);
	goto fail;
fail:
	free_sparse_list(list);
	return NULL;
}
