/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "util.h"
#include "tar.h"

#include <sys/sysmacros.h>
#include <string.h>
#include <stdio.h>

static unsigned long pax_hdr_counter = 0;
static char buffer[4096];

static void write_octal(char *dst, unsigned int value, int digits)
{
	char temp[64];

	sprintf(temp, "%0*o ", digits, value);
	memcpy(dst, temp, strlen(temp));
}

static int name_to_tar_header(tar_header_t *hdr, const char *path)
{
	size_t len = strlen(path);
	const char *ptr;

	if ((len + 1) <= sizeof(hdr->name)) {
		memcpy(hdr->name, path, len);
		return 0;
	}

	for (ptr = path; ; ++ptr) {
		ptr = strchr(ptr, '/');
		if (ptr == NULL)
			return -1;

		len = ptr - path;
		if (len >= sizeof(hdr->tail.posix.prefix))
			continue;
		if (strlen(ptr + 1) >= sizeof(hdr->name))
			continue;
		break;
	}

	memcpy(hdr->tail.posix.prefix, path, ptr - path);
	memcpy(hdr->name, ptr + 1, strlen(ptr + 1));
	return 0;
}

static void init_header(tar_header_t *hdr, const struct stat *sb,
			const char *name, const char *slink_target)
{
	memset(hdr, 0, sizeof(*hdr));

	name_to_tar_header(hdr, name);
	memcpy(hdr->magic, TAR_MAGIC, sizeof(hdr->magic));
	memcpy(hdr->version, TAR_VERSION, sizeof(hdr->version));
	write_octal(hdr->mode, sb->st_mode & ~S_IFMT, 6);
	write_octal(hdr->uid, sb->st_uid, 6);
	write_octal(hdr->gid, sb->st_gid, 6);
	write_octal(hdr->mtime, sb->st_mtime, 11);
	write_octal(hdr->size, 0, 11);
	write_octal(hdr->devmajor, 0, 6);
	write_octal(hdr->devminor, 0, 6);

	switch (sb->st_mode & S_IFMT) {
	case S_IFREG:
		write_octal(hdr->size, sb->st_size & 077777777777L, 11);
		break;
	case S_IFLNK:
		if (sb->st_size < (off_t)sizeof(hdr->linkname))
			strcpy(hdr->linkname, slink_target);
		break;
	case S_IFCHR:
	case S_IFBLK:
		write_octal(hdr->devmajor, major(sb->st_rdev), 6);
		write_octal(hdr->devminor, minor(sb->st_rdev), 6);
		break;
	}

	sprintf(hdr->uname, "%u", sb->st_uid);
	sprintf(hdr->gname, "%u", sb->st_gid);
}

static void update_checksum(tar_header_t *hdr)
{
	unsigned int chksum = 0;
	size_t i;

	memset(hdr->chksum, ' ', sizeof(hdr->chksum));

	for (i = 0; i < sizeof(*hdr); ++i)
		chksum += ((unsigned char *)hdr)[i];

	write_octal(hdr->chksum, chksum, 6);
	hdr->chksum[6] = '\0';
	hdr->chksum[7] = ' ';
}

static bool need_pax_header(const struct stat *sb, const char *name)
{
	tar_header_t tmp;

	if (sb->st_uid > 0777777 || sb->st_gid > 0777777)
		return true;

	if (S_ISREG(sb->st_mode) && sb->st_size > 077777777777L)
		return true;

	if (S_ISLNK(sb->st_mode) && sb->st_size >= (off_t)sizeof(tmp.linkname))
		return true;

	if (name_to_tar_header(&tmp, name))
		return true;

	return false;
}

static char *write_pax_entry(char *dst, const char *key, const char *value)
{
	size_t i, len, prefix = 0, oldprefix;

	do {
		len = prefix + 1 + strlen(key) + 1 + strlen(value) + 1;

		oldprefix = prefix;
		prefix = 1;

		for (i = len; i >= 10; i /= 10)
			++prefix;
	} while (oldprefix != prefix);

	sprintf(dst, "%zu %s=%s\n", len, key, value);

	return dst + len;
}

static int write_pax_header(int fd, const struct stat *sb, const char *name,
			    const char *slink_target)
{
	char temp[64], *ptr;
	struct stat fakesb;
	tar_header_t hdr;
	ssize_t ret;
	size_t len;

	memset(buffer, 0, sizeof(buffer));
	memset(&fakesb, 0, sizeof(fakesb));
	fakesb.st_mode = S_IFREG | 0644;

	sprintf(temp, "pax%lu", pax_hdr_counter);
	init_header(&hdr, &fakesb, temp, NULL);
	hdr.typeflag = TAR_TYPE_PAX;

	sprintf(temp, "%u", sb->st_uid);
	ptr = buffer;
	ptr = write_pax_entry(ptr, "uid", temp);
	ptr = write_pax_entry(ptr, "uname", temp);

	sprintf(temp, "%lu", sb->st_mtime);
	ptr = write_pax_entry(ptr, "mtime", temp);

	sprintf(temp, "%u", sb->st_gid);
	ptr = write_pax_entry(ptr, "gid", temp);
	ptr = write_pax_entry(ptr, "gname", temp);

	ptr = write_pax_entry(ptr, "path", name);

	if (S_ISLNK(sb->st_mode)) {
		ptr = write_pax_entry(ptr, "linkpath", slink_target);
	} else if (S_ISREG(sb->st_mode)) {
		sprintf(temp, "%lu", sb->st_size);
		ptr = write_pax_entry(ptr, "size", temp);
	}

	len = strlen(buffer);
	write_octal(hdr.size, len, 11);
	update_checksum(&hdr);

	ret = write_retry(fd, &hdr, sizeof(hdr));
	if (ret < 0)
		goto fail_wr;
	if ((size_t)ret < sizeof(hdr))
		goto fail_trunc;

	ret = write_retry(fd, buffer, len);
	if (ret < 0)
		goto fail_wr;
	if ((size_t)ret < len)
		goto fail_trunc;

	return padd_file(fd, len, 512);
fail_wr:
	perror("writing pax header");
	return -1;
fail_trunc:
	fputs("writing pax header: truncated write\n", stderr);
	return -1;
}

int write_tar_header(int fd, const struct stat *sb, const char *name,
		     const char *slink_target)
{
	const char *reason;
	tar_header_t hdr;
	ssize_t ret;

	if (need_pax_header(sb, name)) {
		if (write_pax_header(fd, sb, name, slink_target))
			return -1;

		sprintf(buffer, "pax%lu_data", pax_hdr_counter++);
		name = buffer;
	}

	init_header(&hdr, sb, name, slink_target);

	switch (sb->st_mode & S_IFMT) {
	case S_IFCHR: hdr.typeflag = TAR_TYPE_CHARDEV; break;
	case S_IFBLK: hdr.typeflag = TAR_TYPE_BLOCKDEV; break;
	case S_IFLNK: hdr.typeflag = TAR_TYPE_SLINK; break;
	case S_IFREG: hdr.typeflag = TAR_TYPE_FILE; break;
	case S_IFDIR: hdr.typeflag = TAR_TYPE_DIR; break;
	case S_IFIFO: hdr.typeflag = TAR_TYPE_FIFO; break;
	case S_IFSOCK:
		reason = "cannot pack socket";
		goto out_skip;
	default:
		reason = "unknown type";
		goto out_skip;
	}

	update_checksum(&hdr);

	ret = write_retry(fd, &hdr, sizeof(hdr));

	if (ret < 0) {
		perror("writing header record");
	} else if ((size_t)ret < sizeof(hdr)) {
		fputs("writing header record: truncated write\n", stderr);
		ret = -1;
	} else {
		ret = 0;
	}

	return ret;
out_skip:
	fprintf(stderr, "WARNING: skipping '%s' (%s)\n", name, reason);
	return 1;
}
