/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * pax_header.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "internal.h"

static tar_xattr_t *mkxattr(const char *key, size_t keylen,
			    const char *value, size_t valuelen)
{
	tar_xattr_t *xattr;

	xattr = calloc(1, sizeof(*xattr) + keylen + 1 + valuelen + 1);
	if (xattr == NULL)
		return NULL;

	xattr->key = xattr->data;
	xattr->value = (sqfs_u8 *)xattr->data + keylen + 1;
	xattr->value_len = valuelen;
	memcpy(xattr->key, key, keylen);
	memcpy(xattr->value, value, valuelen);
	return xattr;
}

int read_pax_header(FILE *fp, sqfs_u64 entsize, unsigned int *set_by_pax,
		    tar_header_decoded_t *out)
{
	char *buffer, *line, *key, *ptr, *value, *end;
	sparse_map_t *sparse_last = NULL, *sparse;
	sqfs_u64 field, offset = 0, num_bytes = 0;
	tar_xattr_t *xattr;
	long len;

	buffer = record_to_memory(fp, entsize);
	if (buffer == NULL)
		return -1;

	end = buffer + entsize;

	for (line = buffer; line < end; line += len) {
		len = strtol(line, &ptr, 10);
		if (ptr == line || !isspace(*ptr) || len <= 0)
			goto fail_malformed;

		if (len > (end - line))
			goto fail_ov;

		line[len - 1] = '\0';

		while (ptr < end && isspace(*ptr))
			++ptr;

		if (ptr >= end || (ptr - line) >= len)
			goto fail_malformed;

		if (!strncmp(ptr, "uid=", 4)) {
			if (pax_read_decimal(ptr + 4, &field))
				goto fail;
			out->sb.st_uid = field;
			*set_by_pax |= PAX_UID;
		} else if (!strncmp(ptr, "gid=", 4)) {
			if (pax_read_decimal(ptr + 4, &field))
				goto fail;
			out->sb.st_gid = field;
			*set_by_pax |= PAX_GID;
		} else if (!strncmp(ptr, "path=", 5)) {
			free(out->name);
			out->name = strdup(ptr + 5);
			if (out->name == NULL)
				goto fail_errno;
			*set_by_pax |= PAX_NAME;
		} else if (!strncmp(ptr, "size=", 5)) {
			if (pax_read_decimal(ptr + 5, &out->record_size))
				goto fail;
			*set_by_pax |= PAX_SIZE;
		} else if (!strncmp(ptr, "linkpath=", 9)) {
			free(out->link_target);
			out->link_target = strdup(ptr + 9);
			if (out->link_target == NULL)
				goto fail_errno;
			*set_by_pax |= PAX_SLINK_TARGET;
		} else if (!strncmp(ptr, "mtime=", 6)) {
			if (ptr[6] == '-') {
				if (pax_read_decimal(ptr + 7, &field))
					goto fail;
				out->mtime = -((sqfs_s64)field);
			} else {
				if (pax_read_decimal(ptr + 6, &field))
					goto fail;
				out->mtime = field;
			}
			*set_by_pax |= PAX_MTIME;
		} else if (!strncmp(ptr, "GNU.sparse.name=", 16)) {
			free(out->name);
			out->name = strdup(ptr + 16);
			if (out->name == NULL)
				goto fail_errno;
			*set_by_pax |= PAX_NAME;
		} else if (!strncmp(ptr, "GNU.sparse.map=", 15)) {
			free_sparse_list(out->sparse);
			sparse_last = NULL;

			out->sparse = read_sparse_map(ptr + 15);
			if (out->sparse == NULL)
				goto fail;
		} else if (!strncmp(ptr, "GNU.sparse.size=", 16)) {
			if (pax_read_decimal(ptr + 16, &out->actual_size))
				goto fail;
			*set_by_pax |= PAX_SPARSE_SIZE;
		} else if (!strncmp(ptr, "GNU.sparse.realsize=", 20)) {
			if (pax_read_decimal(ptr + 20, &out->actual_size))
				goto fail;
			*set_by_pax |= PAX_SPARSE_SIZE;
		} else if (!strncmp(ptr, "GNU.sparse.major=", 17) ||
			   !strncmp(ptr, "GNU.sparse.minor=", 17)) {
			*set_by_pax |= PAX_SPARSE_GNU_1_X;
		} else if (!strncmp(ptr, "GNU.sparse.offset=", 18)) {
			if (pax_read_decimal(ptr + 18, &offset))
				goto fail;
		} else if (!strncmp(ptr, "GNU.sparse.numbytes=", 20)) {
			if (pax_read_decimal(ptr + 20, &num_bytes))
				goto fail;
			sparse = calloc(1, sizeof(*sparse));
			if (sparse == NULL)
				goto fail_errno;
			sparse->offset = offset;
			sparse->count = num_bytes;
			if (sparse_last == NULL) {
				free_sparse_list(out->sparse);
				out->sparse = sparse_last = sparse;
			} else {
				sparse_last->next = sparse;
				sparse_last = sparse;
			}
		} else if (!strncmp(ptr, "SCHILY.xattr.", 13)) {
			key = ptr + 13;

			ptr = strrchr(key, '=');
			if (ptr == NULL || ptr == key)
				continue;

			value = ptr + 1;

			xattr = mkxattr(key, ptr - key,
					value, len - (value - line) - 1);
			if (xattr == NULL)
				goto fail_errno;

			xattr->next = out->xattr;
			out->xattr = xattr;
		} else if (!strncmp(ptr, "LIBARCHIVE.xattr.", 17)) {
			key = ptr + 17;

			ptr = strrchr(key, '=');
			if (ptr == NULL || ptr == key)
				continue;

			value = ptr + 1;

			xattr = mkxattr(key, ptr - key, value, strlen(value));
			if (xattr == NULL)
				goto fail_errno;

			urldecode(xattr->key);
			xattr->value_len = base64_decode(xattr->value, value,
							 xattr->value_len);

			xattr->next = out->xattr;
			out->xattr = xattr;
		}
	}

	free(buffer);
	return 0;
fail_malformed:
	fputs("Found a malformed PAX header.\n", stderr);
	goto fail;
fail_ov:
	fputs("Numeric overflow in PAX header.\n", stderr);
	goto fail;
fail_errno:
	perror("reading pax header");
	goto fail;
fail:
	free(buffer);
	return -1;
}
