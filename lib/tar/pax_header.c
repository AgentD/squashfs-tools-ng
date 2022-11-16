/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * pax_header.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "internal.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

static sqfs_u8 base64_convert(char in)
{
	if (isupper(in))
		return in - 'A';
	if (islower(in))
		return in - 'a' + 26;
	if (isdigit(in))
		return in - '0' + 52;
	if (in == '+')
		return 62;
	if (in == '/' || in == '-')
		return 63;
	return 0;
}

static size_t base64_decode(sqfs_u8 *out, const char *in, size_t len)
{
	sqfs_u8 *start = out;

	while (len > 0) {
		unsigned int diff = 0, value = 0;

		while (diff < 4 && len > 0) {
			if (*in == '=' || *in == '_' || *in == '\0') {
				len = 0;
			} else {
				value = (value << 6) | base64_convert(*(in++));
				--len;
				++diff;
			}
		}

		if (diff < 2)
			break;

		value <<= 6 * (4 - diff);

		switch (diff) {
		case 4:  out[2] = value & 0xff; /* fall-through */
		case 3:  out[1] = (value >> 8) & 0xff; /* fall-through */
		default: out[0] = (value >> 16) & 0xff;
		}

		out += (diff * 3) / 4;
	}

	*out = '\0';
	return out - start;
}

static int pax_read_decimal(const char *str, sqfs_u64 *out)
{
	sqfs_u64 result = 0;

	while (*str >= '0' && *str <= '9') {
		if (result > 0xFFFFFFFFFFFFFFFFUL / 10) {
			fputs("numeric overflow parsing pax header\n", stderr);
			return -1;
		}

		result = (result * 10) + (*(str++) - '0');
	}

	*out = result;
	return 0;
}

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

	free_sparse_list(out->sparse);
	out->sparse = NULL;

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

static int pax_xattr_schily(tar_header_decoded_t *out,
			    tar_xattr_t *xattr)
{
	xattr->next = out->xattr;
	out->xattr = xattr;
	return 0;
}

static int pax_xattr_libarchive(tar_header_decoded_t *out,
				tar_xattr_t *xattr)
{
	urldecode(xattr->key);
	xattr->value_len = base64_decode(xattr->value,
					 (const char *)xattr->value,
					 xattr->value_len);
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
		int (*xattr)(tar_header_decoded_t *out, tar_xattr_t *xattr);
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

static tar_xattr_t *mkxattr(const char *key,
			    const char *value, size_t valuelen)
{
	size_t keylen = strlen(key);
	tar_xattr_t *xattr;

	xattr = calloc(1, sizeof(*xattr) + keylen + 1 + valuelen + 1);
	if (xattr == NULL)
		return NULL;

	xattr->key = xattr->data;
	memcpy(xattr->key, key, keylen);
	xattr->key[keylen] = '\0';

	xattr->value = (sqfs_u8 *)xattr->key + keylen + 1;
	memcpy(xattr->value, value, valuelen);
	xattr->value[valuelen] = '\0';

	xattr->value_len = valuelen;
	return xattr;
}

static int apply_handler(tar_header_decoded_t *out,
			 const struct pax_handler_t *field, const char *key,
			 const char *value, size_t valuelen)
{
	tar_xattr_t *xattr;
	sqfs_s64 s64val;
	sqfs_u64 uval;
	char *copy;

	switch (field->type) {
	case PAX_TYPE_SINT:
		if (value[0] == '-') {
			if (pax_read_decimal(value + 1, &uval))
				return -1;
			s64val = -((sqfs_s64)uval);
		} else {
			if (pax_read_decimal(value, &uval))
				return -1;
			s64val = (sqfs_s64)uval;
		}
		return field->cb.sint(out, s64val);
	case PAX_TYPE_UINT:
		if (pax_read_decimal(value, &uval))
			return -1;
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
		xattr = mkxattr(key + strlen(field->name) + 1,
				value, valuelen);
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
}

int read_pax_header(istream_t *fp, sqfs_u64 entsize, unsigned int *set_by_pax,
		    tar_header_decoded_t *out)
{
	char *buffer, *line, *key, *ptr, *value, *end;
	sparse_map_t *sparse_last = NULL, *sparse;
	sqfs_u64 offset = 0, num_bytes = 0;
	const struct pax_handler_t *field;
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
			if (pax_read_decimal(value, &offset))
				goto fail;
		} else if (!strcmp(key, "GNU.sparse.numbytes")) {
			if (pax_read_decimal(value, &num_bytes))
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
