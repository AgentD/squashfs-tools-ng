/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "util.h"
#include "tar.h"

#include <sys/sysmacros.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>

enum {
	PAX_SIZE = 0x001,
	PAX_UID = 0x002,
	PAX_GID = 0x004,
	PAX_DEV_MAJ = 0x008,
	PAX_DEV_MIN = 0x010,
	PAX_NAME = 0x020,
	PAX_SLINK_TARGET = 0x040,
	PAX_ATIME = 0x080,
	PAX_MTIME = 0x100,
	PAX_CTIME = 0x200,
};

static int read_octal(const char *str, int digits, uint64_t *out)
{
	uint64_t result = 0;

	while (digits > 0 && *str >= '0' && *str <= '7') {
		if (result > 0x1FFFFFFFFFFFFFFFUL) {
			fputs("numeric overflow parsing tar header\n", stderr);
			return -1;
		}

		result = (result << 3) | (*(str++) - '0');
		--digits;
	}

	*out = result;
	return 0;
}

static int read_binary(const char *str, int digits, uint64_t *out)
{
	uint64_t x, result = 0;

	while (digits > 0) {
		if (result > 0x00FFFFFFFFFFFFFFUL) {
			fputs("numeric overflow parsing tar header\n", stderr);
			return -1;
		}

		x = *((const unsigned char *)str++);
		result = (result << 8) | x;
		--digits;
	}

	*out = result;
	return 0;
}

static int read_number(const char *str, int digits, uint64_t *out)
{
	if (*((unsigned char *)str) & 0x80) {
		if (read_binary(str, digits, out))
			return -1;
		*out &= 0x7FFFFFFFFFFFFFFF;
		return 0;
	}

	return read_octal(str, digits, out);
}

static bool is_zero_block(const tar_header_t *hdr)
{
	const unsigned char *ptr = (const unsigned char *)hdr;

	return ptr[0] == '\0' && memcmp(ptr, ptr + 1, sizeof(*hdr) - 1) == 0;
}

static bool is_checksum_valid(const tar_header_t *hdr)
{
	unsigned int chksum = 0;
	tar_header_t copy;
	uint64_t ref;
	size_t i;

	if (read_octal(hdr->chksum, sizeof(hdr->chksum), &ref))
		return false;

	memcpy(&copy, hdr, sizeof(*hdr));
	memset(copy.chksum, ' ', sizeof(copy.chksum));

	for (i = 0; i < sizeof(copy); ++i)
		chksum += ((unsigned char *)&copy)[i];

	return chksum == ref;
}

static bool is_magic_valid(const tar_header_t *hdr)
{
	if (memcmp(hdr->magic, TAR_MAGIC, sizeof(hdr->magic)) != 0)
		return false;

	if (memcmp(hdr->version, TAR_VERSION, sizeof(hdr->version)) != 0)
		return false;

	return true;
}

static int pax_read_decimal(const char *str, uint64_t *out)
{
	uint64_t result = 0;

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

static int read_pax_header(int fd, uint64_t entsize, unsigned int *set_by_pax,
			   tar_header_decoded_t *out)
{
	char *buffer, *line;
	uint64_t field;
	ssize_t ret;
	uint64_t i;

	buffer = malloc(entsize + 1);
	if (buffer == NULL)
		goto fail_errno;

	ret = read_retry(fd, buffer, entsize);
	if (ret < 0)
		goto fail_errno;
	if ((size_t)ret < entsize)
		goto fail_eof;

	if (skip_padding(fd, entsize))
		goto fail;

	buffer[entsize] = '\0';

	for (i = 0; i < entsize; ++i) {
		while (i < entsize && isspace(buffer[i]))
			++i;
		while (i < entsize && isdigit(buffer[i]))
			++i;
		while (i < entsize && isspace(buffer[i]))
			++i;
		if (i >= entsize)
			break;

		line = buffer + i;

		while (i < entsize && buffer[i] != '\n')
			++i;

		buffer[i] = '\0';

		if (!strncmp(line, "uid=", 4)) {
			pax_read_decimal(line + 4, &field);
			out->sb.st_uid = field;
			*set_by_pax |= PAX_UID;
		} else if (!strncmp(line, "gid=", 4)) {
			pax_read_decimal(line + 4, &field);
			out->sb.st_gid = field;
			*set_by_pax |= PAX_GID;
		} else if (!strncmp(line, "path=", 5)) {
			free(out->name);
			out->name = strdup(line + 5);
			if (out->name == NULL)
				goto fail_errno;
			*set_by_pax |= PAX_NAME;
		} else if (!strncmp(line, "size=", 5)) {
			pax_read_decimal(line + 5, &field);
			out->sb.st_size = field;
			*set_by_pax |= PAX_SIZE;
		} else if (!strncmp(line, "linkpath=", 9)) {
			free(out->link_target);
			out->link_target = strdup(line + 9);
			if (out->link_target == NULL)
				goto fail_errno;
			*set_by_pax |= PAX_SLINK_TARGET;
		} else if (!strncmp(line, "atime=", 6)) {
			pax_read_decimal(line + 6, &field);
			out->sb.st_atime = field;
			*set_by_pax |= PAX_ATIME;
		} else if (!strncmp(line, "mtime=", 6)) {
			pax_read_decimal(line + 6, &field);
			out->sb.st_mtime = field;
			*set_by_pax |= PAX_MTIME;
		} else if (!strncmp(line, "ctime=", 6)) {
			pax_read_decimal(line + 6, &field);
			out->sb.st_ctime = field;
			*set_by_pax |= PAX_CTIME;
		}
	}

	free(buffer);
	return 0;
fail_errno:
	perror("reading pax header");
	goto fail;
fail_eof:
	fputs("reading pax header: unexpected end of file\n", stderr);
	goto fail;
fail:
	free(buffer);
	return -1;
}

static int decode_header(const tar_header_t *hdr, unsigned int set_by_pax,
			 tar_header_decoded_t *out)
{
	uint64_t field;
	size_t count;

	if (!(set_by_pax & PAX_NAME)) {
		if (hdr->prefix[0] == '\0') {
			count = strlen(hdr->name) + 1;
			count += strlen(hdr->prefix) + 1;

			out->name = malloc(count);

			if (out->name != NULL) {
				sprintf(out->name, "%s/%s",
					hdr->prefix, hdr->name);
			}
		} else {
			out->name = strdup(hdr->name);
		}

		if (out->name == NULL) {
			perror("decoding filename");
			return -1;
		}
	}

	if (!(set_by_pax & PAX_SIZE)) {
		if (read_number(hdr->size, sizeof(hdr->size), &field))
			return -1;
		out->sb.st_size = field;
	}

	if (!(set_by_pax & PAX_UID)) {
		if (read_number(hdr->uid, sizeof(hdr->uid), &field))
			return -1;
		out->sb.st_uid = field;
	}

	if (!(set_by_pax & PAX_GID)) {
		if (read_number(hdr->gid, sizeof(hdr->gid), &field))
			return -1;
		out->sb.st_gid = field;
	}

	if (!(set_by_pax & PAX_DEV_MAJ)) {
		if (read_number(hdr->devmajor, sizeof(hdr->devmajor), &field))
			return -1;

		out->sb.st_rdev = makedev(field, minor(out->sb.st_rdev));
	}

	if (!(set_by_pax & PAX_DEV_MIN)) {
		if (read_number(hdr->devminor, sizeof(hdr->devminor), &field))
			return -1;

		out->sb.st_rdev = makedev(major(out->sb.st_rdev), field);
	}

	if (!(set_by_pax & PAX_MTIME)) {
		if (read_number(hdr->mtime, sizeof(hdr->mtime), &field))
			return -1;
		out->sb.st_mtime = field;
	}

	if (!(set_by_pax & PAX_ATIME))
		out->sb.st_atime = out->sb.st_mtime;

	if (!(set_by_pax & PAX_CTIME))
		out->sb.st_ctime = out->sb.st_mtime;

	if (read_octal(hdr->mode, sizeof(hdr->mode), &field))
		return -1;

	out->sb.st_mode = field & 07777;

	if (hdr->typeflag == TAR_TYPE_LINK ||
	    hdr->typeflag == TAR_TYPE_SLINK) {
		if (!(set_by_pax & PAX_SLINK_TARGET)) {
			out->link_target = strdup(hdr->linkname);
			if (out->link_target == NULL) {
				perror("decoding symlink target");
				return -1;
			}
		}
	}

	out->unknown_record = false;

	switch (hdr->typeflag) {
	case '\0':
	case TAR_TYPE_FILE:
		out->sb.st_mode |= S_IFREG;
		break;
	case TAR_TYPE_LINK:
		/* XXX: hard links are not support yet */
		out->sb.st_mode = S_IFLNK | 0777;
		break;
	case TAR_TYPE_SLINK:
		out->sb.st_mode = S_IFLNK | 0777;
		break;
	case TAR_TYPE_CHARDEV:
		out->sb.st_mode |= S_IFCHR;
		break;
	case TAR_TYPE_BLOCKDEV:
		out->sb.st_mode |= S_IFBLK;
		break;
	case TAR_TYPE_DIR:
		out->sb.st_mode |= S_IFDIR;
		break;
	case TAR_TYPE_FIFO:
		out->sb.st_mode |= S_IFIFO;
		break;
	default:
		out->unknown_record = true;
		break;
	}

	return 0;
}

int read_header(int fd, tar_header_decoded_t *out)
{
	unsigned int set_by_pax = 0;
	bool prev_was_zero = false;
	uint64_t pax_size;
	tar_header_t hdr;
	int ret;

	memset(out, 0, sizeof(*out));

	for (;;) {
		ret = read_retry(fd, &hdr, sizeof(hdr));
		if (ret == 0)
			goto out_eof;
		if (ret < 0)
			goto fail_errno;
		if (ret < (int)sizeof(hdr))
			goto fail_eof;

		if (is_zero_block(&hdr)) {
			if (prev_was_zero)
				goto out_eof;
			prev_was_zero = true;
			continue;
		}

		prev_was_zero = false;

		if (!is_magic_valid(&hdr))
			goto fail_magic;

		if (!is_checksum_valid(&hdr))
			goto fail_chksum;

		if (hdr.typeflag == TAR_TYPE_PAX) {
			clear_header(out);
			if (read_number(hdr.size, sizeof(hdr.size), &pax_size))
				return -1;
			set_by_pax = 0;
			if (read_pax_header(fd, pax_size, &set_by_pax, out))
				return -1;
			continue;
		}
		break;
	}

	if (decode_header(&hdr, set_by_pax, out))
		goto fail;

	return 0;
out_eof:
	clear_header(out);
	return 1;
fail_errno:
	perror("reading tar header");
	goto fail;
fail_eof:
	fputs("reading tar header: unexpected end of file\n", stderr);
	goto fail;
fail_magic:
	fputs("input is not a ustar tar archive!\n", stderr);
	goto fail;
fail_chksum:
	fputs("invalid tar header checksum!\n", stderr);
	goto fail;
fail:
	clear_header(out);
	return -1;
}

void clear_header(tar_header_decoded_t *hdr)
{
	free(hdr->name);
	free(hdr->link_target);
	memset(hdr, 0, sizeof(*hdr));
}
