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

static void init_header(tar_header_t *hdr, const fstree_t *fs,
			const tree_node_t *n)
{
	memset(hdr, 0, sizeof(*hdr));

	memcpy(hdr->magic, TAR_MAGIC, strlen(TAR_MAGIC));
	memcpy(hdr->version, TAR_VERSION, strlen(TAR_VERSION));

	write_octal(hdr->mode, n->mode & ~S_IFMT, 6);
	write_octal(hdr->uid, n->uid, 6);
	write_octal(hdr->gid, n->gid, 6);
	write_octal(hdr->size, 0, 11);
	write_octal(hdr->mtime, fs->default_mtime, 11);
	write_octal(hdr->devmajor, 0, 6);
	write_octal(hdr->devminor, 0, 6);

	sprintf(hdr->uname, "%u", n->uid);
	sprintf(hdr->gname, "%u", n->gid);
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
		if (len >= sizeof(hdr->prefix))
			continue;
		if (strlen(ptr + 1) >= sizeof(hdr->name))
			continue;
		break;
	}

	memcpy(hdr->prefix, path, ptr - path);
	memcpy(hdr->name, ptr + 1, strlen(ptr + 1));
	return 0;
}

static bool need_pax_header(const tree_node_t *n, const char *name)
{
	tar_header_t temp;

	if (n->uid > 0777777 || n->gid > 0777777)
		return true;

	if (S_ISREG(n->mode) && n->data.file->size > 077777777777UL)
		return true;

	if (S_ISLNK(n->mode)) {
		if (strlen(n->data.slink_target) >= sizeof(temp.linkname))
			return true;
	}

	if (name_to_tar_header(&temp, name))
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

static int write_pax_header(int fd, const fstree_t *fs, const tree_node_t *n,
			    const char *name)
{
	char temp[64], *ptr;
	tar_header_t hdr;
	ssize_t ret;
	size_t len;

	memset(buffer, 0, sizeof(buffer));
	memset(&hdr, 0, sizeof(hdr));

	sprintf(hdr.name, "pax%lu", pax_hdr_counter);
	memcpy(hdr.magic, TAR_MAGIC, strlen(TAR_MAGIC));
	memcpy(hdr.version, TAR_VERSION, strlen(TAR_VERSION));
	write_octal(hdr.mode, 0644, 6);
	write_octal(hdr.uid, 0, 6);
	write_octal(hdr.gid, 0, 6);
	write_octal(hdr.mtime, 0, 11);
	write_octal(hdr.devmajor, 0, 6);
	write_octal(hdr.devminor, 0, 6);
	sprintf(hdr.uname, "%u", 0);
	sprintf(hdr.gname, "%u", 0);
	hdr.typeflag = TAR_TYPE_PAX;

	ptr = buffer;
	sprintf(temp, "%u", n->uid);
	ptr = write_pax_entry(ptr, "uid", temp);
	ptr = write_pax_entry(ptr, "uname", temp);

	sprintf(temp, "%u", fs->default_mtime);
	ptr = write_pax_entry(ptr, "mtime", temp);

	sprintf(temp, "%u", n->gid);
	ptr = write_pax_entry(ptr, "gid", temp);
	ptr = write_pax_entry(ptr, "gname", temp);

	ptr = write_pax_entry(ptr, "path", name);

	if (S_ISLNK(n->mode)) {
		ptr = write_pax_entry(ptr, "linkpath", n->data.slink_target);
	} else if (S_ISREG(n->mode)) {
		sprintf(temp, "%lu", n->data.file->size);
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

int write_tar_header(int fd, const fstree_t *fs, const tree_node_t *n,
		     const char *name)
{
	const char *reason;
	tar_header_t hdr;
	ssize_t ret;

	if (need_pax_header(n, name)) {
		if (write_pax_header(fd, fs, n, name))
			return -1;

		init_header(&hdr, fs, n);
		sprintf(hdr.name, "pax%lu_data", pax_hdr_counter++);
	} else {
		init_header(&hdr, fs, n);
		name_to_tar_header(&hdr, name);
	}

	switch (n->mode & S_IFMT) {
	case S_IFCHR:
	case S_IFBLK:
		write_octal(hdr.devmajor, major(n->data.devno), 6);
		write_octal(hdr.devminor, minor(n->data.devno), 6);
		hdr.typeflag = S_ISBLK(n->mode) ? TAR_TYPE_BLOCKDEV :
			TAR_TYPE_CHARDEV;
		break;
	case S_IFLNK:
		if (strlen(n->data.slink_target) < sizeof(hdr.linkname))
			strcpy(hdr.linkname, n->data.slink_target);

		hdr.typeflag = TAR_TYPE_SLINK;
		break;
	case S_IFREG:
		write_octal(hdr.size, n->data.file->size & 077777777777UL, 11);
		hdr.typeflag = TAR_TYPE_FILE;
		break;
	case S_IFDIR:
		hdr.typeflag = TAR_TYPE_DIR;
		break;
	case S_IFIFO:
		hdr.typeflag = TAR_TYPE_FIFO;
		break;
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
