/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * write_header.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "internal.h"
#include "sqfs/xattr.h"
#include "sqfs/error.h"
#include <string.h>
#include <stdlib.h>

static void update_checksum(tar_header_t *hdr)
{
	sprintf(hdr->chksum, "%06o", tar_compute_checksum(hdr));
	hdr->chksum[6] = '\0';
	hdr->chksum[7] = ' ';
}

static void write_binary(char *dst, sqfs_u64 value, int digits)
{
	memset(dst, 0, digits);

	while (digits > 0) {
		((unsigned char *)dst)[digits - 1] = value & 0xFF;
		--digits;
		value >>= 8;
	}

	((unsigned char *)dst)[0] |= 0x80;
}

static void write_number(char *dst, sqfs_u64 value, int digits)
{
	sqfs_u64 mask = 0;
	char buffer[64];
	int i;

	for (i = 0; i < (digits - 1); ++i)
		mask = (mask << 3) | 7;

	if (value <= mask) {
		sprintf(buffer, "%0*lo ", digits - 1, (unsigned long)value);
		memcpy(dst, buffer, digits);
	} else if (value <= ((mask << 3) | 7)) {
		sprintf(buffer, "%0*lo", digits, (unsigned long)value);
		memcpy(dst, buffer, digits);
	} else {
		write_binary(dst, value, digits);
	}
}

static void write_number_signed(char *dst, sqfs_s64 value, int digits)
{
	sqfs_u64 neg;

	if (value < 0) {
		neg = -value;
		write_binary(dst, ~neg + 1, digits);
	} else {
		write_number(dst, value, digits);
	}
}

static int write_header(sqfs_ostream_t *fp, const struct stat *sb,
			const char *name, const char *slink_target, int type)
{
	int maj = 0, min = 0;
	sqfs_u64 size = 0;
	tar_header_t hdr;

	if (S_ISCHR(sb->st_mode) || S_ISBLK(sb->st_mode)) {
		maj = major(sb->st_rdev);
		min = minor(sb->st_rdev);
	}

	if (S_ISREG(sb->st_mode))
		size = sb->st_size;

	memset(&hdr, 0, sizeof(hdr));

	strncpy(hdr.name, name, sizeof(hdr.name) - 1);
	write_number(hdr.mode, sb->st_mode & ~S_IFMT, sizeof(hdr.mode));
	write_number(hdr.uid, sb->st_uid, sizeof(hdr.uid));
	write_number(hdr.gid, sb->st_gid, sizeof(hdr.gid));
	write_number(hdr.size, size, sizeof(hdr.size));
	write_number_signed(hdr.mtime, sb->st_mtime, sizeof(hdr.mtime));
	hdr.typeflag = type;
	if (slink_target != NULL)
		memcpy(hdr.linkname, slink_target, sb->st_size);
	memcpy(hdr.magic, TAR_MAGIC_OLD, sizeof(hdr.magic));
	memcpy(hdr.version, TAR_VERSION_OLD, sizeof(hdr.version));
	sprintf(hdr.uname, "%u", sb->st_uid);
	sprintf(hdr.gname, "%u", sb->st_gid);
	write_number(hdr.devmajor, maj, sizeof(hdr.devmajor));
	write_number(hdr.devminor, min, sizeof(hdr.devminor));

	update_checksum(&hdr);

	return fp->append(fp, &hdr, sizeof(hdr));
}

static int write_ext_header(sqfs_ostream_t *fp, const struct stat *orig,
			    const char *payload, size_t payload_len,
			    int type, const char *name)
{
	struct stat sb;
	int ret;

	sb = *orig;
	sb.st_mode = S_IFREG | 0644;
	sb.st_size = payload_len;

	ret = write_header(fp, &sb, name, NULL, type);
	if (ret)
		return ret;

	ret = fp->append(fp, payload, payload_len);
	if (ret)
		return ret;

	return padd_file(fp, payload_len);
}

static size_t num_digits(size_t num)
{
	size_t i = 1;

	while (num >= 10) {
		num /= 10;
		++i;
	}

	return i;
}

static size_t prefix_digit_len(size_t len)
{
	size_t old_ndigit, ndigit = 0;

	do {
		old_ndigit = ndigit;
		ndigit = num_digits(len + ndigit);
	} while (old_ndigit != ndigit);

	return ndigit;
}

static int write_schily_xattr(sqfs_ostream_t *fp, const struct stat *orig,
			      const char *name, const sqfs_xattr_t *xattr)
{
	static const char *prefix = "SCHILY.xattr.";
	size_t len, total_size = 0;
	char *buffer, *ptr;
	int ret;

	for (const sqfs_xattr_t *it = xattr; it != NULL; it = it->next) {
		len = strlen(prefix) + strlen(it->key) + it->value_len + 3;

		total_size += len + prefix_digit_len(len);
	}

	buffer = calloc(1, total_size + 1);
	if (buffer == NULL)
		return SQFS_ERROR_ALLOC;

	ptr = buffer;

	for (const sqfs_xattr_t *it = xattr; it != NULL; it = it->next) {
		len = strlen(prefix) + strlen(it->key) + it->value_len + 3;
		len += prefix_digit_len(len);

		sprintf(ptr, PRI_SZ " %s%s=", len, prefix, it->key);
		ptr += strlen(ptr);
		memcpy(ptr, it->value, it->value_len);
		ptr += it->value_len;
		*(ptr++) = '\n';
	}

	ret = write_ext_header(fp, orig, buffer, total_size, TAR_TYPE_PAX, name);
	free(buffer);
	return ret;
}

int write_tar_header(sqfs_ostream_t *fp,
		     const struct stat *sb, const char *name,
		     const char *slink_target, const sqfs_xattr_t *xattr,
		     unsigned int counter)
{
	char buffer[64];
	int type, ret;

	if (xattr != NULL) {
		sprintf(buffer, "pax/xattr%u", counter);

		ret = write_schily_xattr(fp, sb, buffer, xattr);
		if (ret)
			return ret;
	}

	if (!S_ISLNK(sb->st_mode))
		slink_target = NULL;

	if (S_ISLNK(sb->st_mode) && sb->st_size >= 100) {
		sprintf(buffer, "gnu/target%u", counter);
		ret = write_ext_header(fp, sb, slink_target, sb->st_size,
				       TAR_TYPE_GNU_SLINK, buffer);
		if (ret)
			return ret;
		slink_target = NULL;
	}

	if (strlen(name) >= 100) {
		sprintf(buffer, "gnu/name%u", counter);

		ret = write_ext_header(fp, sb, name, strlen(name),
				       TAR_TYPE_GNU_PATH, buffer);
		if (ret)
			return ret;

		sprintf(buffer, "gnu/data%u", counter);
		name = buffer;
	}

	switch (sb->st_mode & S_IFMT) {
	case S_IFCHR: type = TAR_TYPE_CHARDEV; break;
	case S_IFBLK: type = TAR_TYPE_BLOCKDEV; break;
	case S_IFLNK: type = TAR_TYPE_SLINK; break;
	case S_IFREG: type = TAR_TYPE_FILE; break;
	case S_IFDIR: type = TAR_TYPE_DIR; break;
	case S_IFIFO: type = TAR_TYPE_FIFO; break;
	default:
		return SQFS_ERROR_UNSUPPORTED;
	}

	return write_header(fp, sb, name, slink_target, type);
}

int write_hard_link(sqfs_ostream_t *fp, const struct stat *sb, const char *name,
		    const char *target, unsigned int counter)
{
	tar_header_t hdr;
	char buffer[64];
	size_t len;
	int ret;

	memset(&hdr, 0, sizeof(hdr));

	len = strlen(target);
	if (len >= 100) {
		sprintf(buffer, "gnu/target%u", counter);
		ret = write_ext_header(fp, sb, target, len,
				       TAR_TYPE_GNU_SLINK, buffer);
		if (ret)
			return ret;
		sprintf(hdr.linkname, "hardlink_%u", counter);
	} else {
		memcpy(hdr.linkname, target, len);
	}

	len = strlen(name);
	if (len >= 100) {
		sprintf(buffer, "gnu/name%u", counter);
		ret = write_ext_header(fp, sb, name, len,
				       TAR_TYPE_GNU_PATH, buffer);
		if (ret)
			return ret;
		sprintf(hdr.name, "gnu/data%u", counter);
	} else {
		memcpy(hdr.name, name, len);
	}

	write_number(hdr.mode, sb->st_mode & ~S_IFMT, sizeof(hdr.mode));
	write_number(hdr.uid, sb->st_uid, sizeof(hdr.uid));
	write_number(hdr.gid, sb->st_gid, sizeof(hdr.gid));
	write_number(hdr.size, 0, sizeof(hdr.size));
	write_number_signed(hdr.mtime, sb->st_mtime, sizeof(hdr.mtime));
	hdr.typeflag = TAR_TYPE_LINK;
	memcpy(hdr.magic, TAR_MAGIC_OLD, sizeof(hdr.magic));
	memcpy(hdr.version, TAR_VERSION_OLD, sizeof(hdr.version));
	sprintf(hdr.uname, "%u", sb->st_uid);
	sprintf(hdr.gname, "%u", sb->st_gid);
	write_number(hdr.devmajor, 0, sizeof(hdr.devmajor));
	write_number(hdr.devminor, 0, sizeof(hdr.devminor));

	update_checksum(&hdr);
	return fp->append(fp, &hdr, sizeof(hdr));
}
