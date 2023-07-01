/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * pax_header.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "internal.h"
#include "sqfs/xattr.h"
#include "util/parse.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

static void urldecode(char *str)
{
	unsigned char *out = (unsigned char *)str;
	char *in = str;

	while (*in != '\0') {
		sqfs_u8 x = *(in++);

		if (x == '%' && isxdigit(in[0]) && isxdigit(in[1])) {
			hex_decode(in, 2, &x, 1);
			in += 2;
		}

		*(out++) = x;
	}

	*out = '\0';
}

static int pax_uid(tar_header_decoded_t *out, sqfs_u64 id)
{
	out->uid = id;
	return 0;
}

static int pax_gid(tar_header_decoded_t *out, sqfs_u64 id)
{
	out->gid = id;
	return 0;
}

static int pax_size(tar_header_decoded_t *out, sqfs_u64 size)
{
	out->record_size = size;
	return 0;
}

static int pax_mtime(tar_header_decoded_t *out, sqfs_s64 mtime)
{
	out->mtime = mtime;
	return 0;
}

static int pax_rsize(tar_header_decoded_t *out, sqfs_u64 size)
{
	out->actual_size = size;
	return 0;
}

static int pax_path(tar_header_decoded_t *out, char *path)
{
	free(out->name);
	out->name = path;
	return 0;
}

static int pax_slink(tar_header_decoded_t *out, char *path)
{
	free(out->link_target);
	out->link_target = path;
	return 0;
}

static int pax_sparse_map(tar_header_decoded_t *out, const char *line)
{
	sparse_map_t *last = NULL, *list = NULL, *ent = NULL;
	size_t diff;

	free_sparse_list(out->sparse);
	out->sparse = NULL;

	do {
		ent = calloc(1, sizeof(*ent));
		if (ent == NULL)
			goto fail_errno;

		if (parse_uint(line, -1, &diff, 0, 0, &ent->offset))
			goto fail_format;

		if (line[diff] != ',')
			goto fail_format;

		line += diff + 1;
		if (parse_uint(line, -1, &diff, 0, 0, &ent->count))
			goto fail_format;

		line += diff;

		if (last == NULL) {
			list = last = ent;
		} else {
			last->next = ent;
			last = ent;
		}
	} while (*(line++) == ',');

	out->sparse = list;
	return 0;
fail_errno:
	perror("parsing GNU pax sparse file record");
	goto fail;
fail_format:
	fputs("malformed GNU pax sparse file record\n", stderr);
	goto fail;
fail:
	free_sparse_list(list);
	free(ent);
	return -1;
}

static int pax_xattr_schily(tar_header_decoded_t *out, sqfs_xattr_t *xattr)
{
	xattr->next = out->xattr;
	out->xattr = xattr;
	return 0;
}

static int pax_xattr_libarchive(tar_header_decoded_t *out, sqfs_xattr_t *xattr)
{
	char *key = (char *)xattr->data;
	sqfs_u8 *value = xattr->data + (size_t)(xattr->value - xattr->data);
	int ret;

	ret = base64_decode((const char *)xattr->value, xattr->value_len,
			    value, &xattr->value_len);
	if (ret)
		return -1;

	urldecode(key);
	value[xattr->value_len] = '\0';

	xattr->next = out->xattr;
	out->xattr = xattr;
	return 0;
}

enum {
	PAX_TYPE_SINT = 0,
	PAX_TYPE_UINT,
	PAX_TYPE_STRING,
	PAX_TYPE_CONST_STRING,
	PAX_TYPE_PREFIXED_XATTR,
	PAX_TYPE_IGNORE,
};

static const struct pax_handler_t {
	const char *name;
	int flag;
	int type;
	union {
		int (*sint)(tar_header_decoded_t *out, sqfs_s64 sval);
		int (*uint)(tar_header_decoded_t *out, sqfs_u64 uval);
		int (*str)(tar_header_decoded_t *out, char *str);
		int (*cstr)(tar_header_decoded_t *out, const char *str);
		int (*xattr)(tar_header_decoded_t *out, sqfs_xattr_t *xattr);
	} cb;
} pax_fields[] = {
	{ "uid", PAX_UID, PAX_TYPE_UINT, { .uint = pax_uid } },
	{ "gid", PAX_GID, PAX_TYPE_UINT, { .uint = pax_gid } },
	{ "path", PAX_NAME, PAX_TYPE_STRING, { .str = pax_path } },
	{ "size", PAX_SIZE, PAX_TYPE_UINT, { .uint = pax_size } },
	{ "linkpath", PAX_SLINK_TARGET, PAX_TYPE_STRING, { .str = pax_slink } },
	{ "mtime", PAX_MTIME, PAX_TYPE_SINT, { .sint = pax_mtime } },
	{ "GNU.sparse.name", PAX_NAME, PAX_TYPE_STRING, { .str = pax_path } },
	{ "GNU.sparse.size", PAX_SPARSE_SIZE, PAX_TYPE_UINT,
	  {.uint = pax_rsize} },
	{ "GNU.sparse.realsize", PAX_SPARSE_SIZE, PAX_TYPE_UINT,
	  {.uint = pax_rsize} },
	{ "GNU.sparse.major", PAX_SPARSE_GNU_1_X, PAX_TYPE_IGNORE,
	  { .str = NULL } },
	{ "GNU.sparse.minor", PAX_SPARSE_GNU_1_X, PAX_TYPE_IGNORE,
	  { .str = NULL }},
	{ "SCHILY.xattr", 0, PAX_TYPE_PREFIXED_XATTR,
	  { .xattr = pax_xattr_schily } },
	{ "LIBARCHIVE.xattr", 0, PAX_TYPE_PREFIXED_XATTR,
	  { .xattr = pax_xattr_libarchive } },
	{ "GNU.sparse.map", 0, PAX_TYPE_CONST_STRING,
	  { .cstr = pax_sparse_map } },
};

static const struct pax_handler_t *find_handler(const char *key)
{
	size_t i, fieldlen;

	for (i = 0; i < sizeof(pax_fields) / sizeof(pax_fields[0]); ++i) {
		if (pax_fields[i].type == PAX_TYPE_PREFIXED_XATTR) {
			fieldlen = strlen(pax_fields[i].name);

			if (strncmp(key, pax_fields[i].name, fieldlen))
				continue;

			if (key[fieldlen] != '.')
				continue;

			return pax_fields + i;
		}

		if (!strcmp(key, pax_fields[i].name))
			return pax_fields + i;
	}

	return NULL;
}

static int apply_handler(tar_header_decoded_t *out,
			 const struct pax_handler_t *field, const char *key,
			 const char *value, size_t valuelen)
{
	sqfs_xattr_t *xattr;
	sqfs_s64 s64val;
	sqfs_u64 uval;
	size_t diff;
	char *copy;

	switch (field->type) {
	case PAX_TYPE_SINT:
		if (parse_int(value, -1, &diff, 0, 0, &s64val))
			goto fail_numeric;
		return field->cb.sint(out, s64val);
	case PAX_TYPE_UINT:
		if (parse_uint(value, -1, &diff, 0, 0, &uval))
			goto fail_numeric;
		return field->cb.uint(out, uval);
	case PAX_TYPE_CONST_STRING:
		return field->cb.cstr(out, value);
	case PAX_TYPE_STRING:
		copy = strdup(value);
		if (copy == NULL) {
			perror("processing pax header");
			return -1;
		}
		if (field->cb.str(out, copy)) {
			free(copy);
			return -1;
		}
		break;
	case PAX_TYPE_PREFIXED_XATTR:
		xattr = sqfs_xattr_create(key + strlen(field->name) + 1,
					  (const sqfs_u8 *)value,
					  valuelen);
		if (xattr == NULL) {
			perror("reading pax xattr field");
			return -1;
		}
		if (field->cb.xattr(out, xattr)) {
			free(xattr);
			return -1;
		}
		break;
	default:
		break;
	}

	return 0;
fail_numeric:
	fputs("Malformed decimal value in pax header\n", stderr);
	return -1;
}

int read_pax_header(sqfs_istream_t *fp, sqfs_u64 entsize,
		    unsigned int *set_by_pax, tar_header_decoded_t *out)
{
	char *buffer, *line, *key, *ptr, *value, *end;
	sparse_map_t *sparse_last = NULL, *sparse;
	sqfs_u64 offset = 0, num_bytes = 0;
	const struct pax_handler_t *field;
	size_t diff;
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

		key = ptr;

		while (*ptr != '\0' && *ptr != '=')
			++ptr;

		if (ptr == key || *ptr != '=')
			goto fail_malformed;

		*(ptr++) = '\0';
		value = ptr;

		field = find_handler(key);

		if (field != NULL) {
			if (apply_handler(out, field, key, value,
					  len - (value - line) - 1)) {
				goto fail;
			}

			*set_by_pax |= field->flag;
		} else if (!strcmp(key, "GNU.sparse.offset")) {
			if (parse_uint(value, -1, &diff, 0, 0, &offset))
				goto fail_malformed;
		} else if (!strcmp(key, "GNU.sparse.numbytes")) {
			if (parse_uint(value, -1, &diff, 0, 0, &num_bytes))
				goto fail_malformed;
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
