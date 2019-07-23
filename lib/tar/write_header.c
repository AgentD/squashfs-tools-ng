/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"

#include "internal.h"

static void write_binary(char *dst, uint64_t value, int digits)
{
	memset(dst, 0, digits);

	while (digits > 0) {
		((unsigned char *)dst)[digits - 1] = value & 0xFF;
		--digits;
		value >>= 8;
	}

	((unsigned char *)dst)[0] |= 0x80;
}

static void write_number(char *dst, uint64_t value, int digits)
{
	uint64_t mask = 0;
	char buffer[64];
	int i;

	for (i = 0; i < (digits - 1); ++i)
		mask = (mask << 3) | 7;

	if (value <= mask) {
		sprintf(buffer, "%0*o ", digits - 1, (unsigned int)value);
		memcpy(dst, buffer, digits);
	} else if (value <= ((mask << 3) | 7)) {
		sprintf(buffer, "%0*o", digits, (unsigned int)value);
		memcpy(dst, buffer, digits);
	} else {
		write_binary(dst, value, digits);
	}
}

static void write_number_signed(char *dst, int64_t value, int digits)
{
	uint64_t neg;

	if (value < 0) {
		neg = -value;
		write_binary(dst, ~neg + 1, digits);
	} else {
		write_number(dst, value, digits);
	}
}

static int write_header(int fd, const struct stat *sb, const char *name,
			const char *slink_target, int type)
{
	int maj = 0, min = 0;
	uint64_t size = 0;
	tar_header_t hdr;

	if (S_ISCHR(sb->st_mode) || S_ISBLK(sb->st_mode)) {
		maj = major(sb->st_rdev);
		min = minor(sb->st_rdev);
	}

	if (S_ISREG(sb->st_mode))
		size = sb->st_size;

	memset(&hdr, 0, sizeof(hdr));

	memcpy(hdr.name, name, strlen(name));
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

	return write_data("writing tar header record", fd, &hdr, sizeof(hdr));
}

static int write_gnu_header(int fd, const struct stat *orig,
			    const char *payload, size_t payload_len,
			    int type, const char *name)
{
	struct stat sb;

	sb = *orig;
	sb.st_mode = S_IFREG | 0644;
	sb.st_size = payload_len;

	if (write_header(fd, &sb, name, NULL, type))
		return -1;

	if (write_data("writing GNU extension header",
		       fd, payload, payload_len)) {
		return -1;
	}

	return padd_file(fd, payload_len, 512);
}

int write_tar_header(int fd, const struct stat *sb, const char *name,
		     const char *slink_target, unsigned int counter)
{
	const char *reason;
	char buffer[64];
	int type;

	if (!S_ISLNK(sb->st_mode))
		slink_target = NULL;

	if (S_ISLNK(sb->st_mode) && sb->st_size >= 100) {
		sprintf(buffer, "gnu/target%u", counter);
		if (write_gnu_header(fd, sb, slink_target, sb->st_size,
				     TAR_TYPE_GNU_SLINK, buffer))
			return -1;
		slink_target = NULL;
	}

	if (strlen(name) >= 100) {
		sprintf(buffer, "gnu/name%u", counter);

		if (write_gnu_header(fd, sb, name, strlen(name),
				     TAR_TYPE_GNU_PATH, buffer)) {
			return -1;
		}

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
	case S_IFSOCK:
		reason = "cannot pack socket";
		goto out_skip;
	default:
		reason = "unknown type";
		goto out_skip;
	}

	return write_header(fd, sb, name, slink_target, type);
out_skip:
	fprintf(stderr, "WARNING: %s: %s\n", name, reason);
	return 1;
}
