/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "internal.h"

static bool is_zero_block(const tar_header_t *hdr)
{
	const unsigned char *ptr = (const unsigned char *)hdr;

	return ptr[0] == '\0' && memcmp(ptr, ptr + 1, sizeof(*hdr) - 1) == 0;
}

static int check_version(const tar_header_t *hdr)
{
	char buffer[sizeof(hdr->magic) + sizeof(hdr->version)];

	memset(buffer, '\0', sizeof(buffer));
	if (memcmp(hdr->magic, buffer, sizeof(hdr->magic)) == 0 &&
	    memcmp(hdr->version, buffer, sizeof(hdr->version)) == 0)
		return ETV_V7_UNIX;

	if (memcmp(hdr->magic, TAR_MAGIC, sizeof(hdr->magic)) == 0 &&
	    memcmp(hdr->version, TAR_VERSION, sizeof(hdr->version)) == 0)
		return ETV_POSIX;

	if (memcmp(hdr->magic, TAR_MAGIC_OLD, sizeof(hdr->magic)) == 0 &&
	    memcmp(hdr->version, TAR_VERSION_OLD, sizeof(hdr->version)) == 0)
		return ETV_PRE_POSIX;

	return ETV_UNKNOWN;
}

static char *record_to_memory(int fd, uint64_t size)
{
	char *buffer = malloc(size + 1);
	ssize_t ret;

	if (buffer == NULL)
		goto fail_errno;

	ret = read_retry(fd, buffer, size);
	if (ret == 0)
		goto fail_eof;
	if (ret < 0)
		goto fail_errno;
	if ((uint64_t)ret < size)
		goto fail_eof;

	if (skip_padding(fd, size))
		goto fail;

	buffer[size] = '\0';
	return buffer;
fail_errno:
	perror("reading tar record");
	goto fail;
fail_eof:
	fputs("reading tar record: unexpected end of file\n", stderr);
	goto fail;
fail:
	free(buffer);
	return NULL;
}

static int read_pax_header(int fd, uint64_t entsize, unsigned int *set_by_pax,
			   tar_header_decoded_t *out)
{
	sparse_map_t *sparse_last = NULL, *sparse;
	uint64_t field, offset = 0, num_bytes = 0;
	char *buffer, *line;
	uint64_t i;

	buffer = record_to_memory(fd, entsize);
	if (buffer == NULL)
		return -1;

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
			if (pax_read_decimal(line + 4, &field))
				goto fail;
			out->sb.st_uid = field;
			*set_by_pax |= PAX_UID;
		} else if (!strncmp(line, "gid=", 4)) {
			if (pax_read_decimal(line + 4, &field))
				goto fail;
			out->sb.st_gid = field;
			*set_by_pax |= PAX_GID;
		} else if (!strncmp(line, "path=", 5)) {
			free(out->name);
			out->name = strdup(line + 5);
			if (out->name == NULL)
				goto fail_errno;
			*set_by_pax |= PAX_NAME;
		} else if (!strncmp(line, "size=", 5)) {
			if (pax_read_decimal(line + 5, &out->record_size))
				goto fail;
			*set_by_pax |= PAX_SIZE;
		} else if (!strncmp(line, "linkpath=", 9)) {
			free(out->link_target);
			out->link_target = strdup(line + 9);
			if (out->link_target == NULL)
				goto fail_errno;
			*set_by_pax |= PAX_SLINK_TARGET;
		} else if (!strncmp(line, "atime=", 6)) {
			if (line[6] == '-') {
				if (pax_read_decimal(line + 7, &field))
					goto fail;
				out->sb.st_atime = -((int64_t)field);
			} else {
				if (pax_read_decimal(line + 6, &field))
					goto fail;
				out->sb.st_atime = field;
			}
			*set_by_pax |= PAX_ATIME;
		} else if (!strncmp(line, "mtime=", 6)) {
			if (line[6] == '-') {
				if (pax_read_decimal(line + 7, &field))
					goto fail;
				out->sb.st_mtime = -((int64_t)field);
			} else {
				if (pax_read_decimal(line + 6, &field))
					goto fail;
				out->sb.st_mtime = field;
			}
			*set_by_pax |= PAX_MTIME;
		} else if (!strncmp(line, "ctime=", 6)) {
			if (line[6] == '-') {
				if (pax_read_decimal(line + 7, &field))
					goto fail;
				out->sb.st_ctime = -((int64_t)field);
			} else {
				if (pax_read_decimal(line + 6, &field))
					goto fail;
				out->sb.st_ctime = field;
			}
			*set_by_pax |= PAX_CTIME;
		} else if (!strncmp(line, "GNU.sparse.name=", 16)) {
			free(out->name);
			out->name = strdup(line + 5);
			if (out->name == NULL)
				goto fail_errno;
			*set_by_pax |= PAX_NAME;
		} else if (!strncmp(line, "GNU.sparse.map=", 15)) {
			free_sparse_list(out->sparse);
			sparse_last = NULL;

			out->sparse = read_sparse_map(line + 15);
			if (out->sparse == NULL)
				goto fail;
		} else if (!strncmp(line, "GNU.sparse.size=", 16)) {
			if (pax_read_decimal(line + 16, &out->actual_size))
				goto fail;
			*set_by_pax |= PAX_SPARSE_SIZE;
		} else if (!strncmp(line, "GNU.sparse.offset=", 18)) {
			if (pax_read_decimal(line + 18, &offset))
				goto fail;
		} else if (!strncmp(line, "GNU.sparse.numbytes=", 20)) {
			if (pax_read_decimal(line + 20, &num_bytes))
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
fail_errno:
	perror("reading pax header");
	goto fail;
fail:
	free(buffer);
	return -1;
}

static int decode_header(const tar_header_t *hdr, unsigned int set_by_pax,
			 tar_header_decoded_t *out, int version)
{
	size_t len1, len2;
	uint64_t field;

	if (!(set_by_pax & PAX_NAME)) {
		if (hdr->tail.posix.prefix[0] != '\0' &&
		    version == ETV_POSIX) {
			len1 = strnlen(hdr->name, sizeof(hdr->name));
			len2 = strnlen(hdr->tail.posix.prefix,
				       sizeof(hdr->tail.posix.prefix));

			out->name = malloc(len1 + 1 + len2 + 1);

			if (out->name != NULL) {
				memcpy(out->name, hdr->name, len1);
				out->name[len1] = '/';
				memcpy(out->name + len1 + 1,
				       hdr->tail.posix.prefix, len2);
				out->name[len1 + 1 + len2] = '\0';
			}
		} else {
			out->name = strndup(hdr->name, sizeof(hdr->name));
		}

		if (out->name == NULL) {
			perror("decoding filename");
			return -1;
		}
	}

	if (!(set_by_pax & PAX_SIZE)) {
		if (read_number(hdr->size, sizeof(hdr->size), &out->record_size))
			return -1;
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
		if (field & 0x8000000000000000UL) {
			field = ~field + 1;
			out->sb.st_mtime = -((int64_t)field);
		} else {
			out->sb.st_mtime = field;
		}
	}

	if (!(set_by_pax & PAX_ATIME)) {
		field = out->sb.st_mtime;

		if (version == ETV_PRE_POSIX &&
		    ((uint8_t)hdr->tail.gnu.atime[0] == 0x80 ||
		     (uint8_t)hdr->tail.gnu.atime[0] == 0xFF ||
		     isdigit(hdr->tail.gnu.atime[0]))) {
			if (read_number(hdr->tail.gnu.atime,
					sizeof(hdr->tail.gnu.atime), &field))
				return -1;
		}

		if (field & 0x8000000000000000UL) {
			field = ~field + 1;
			out->sb.st_atime = -((int64_t)field);
		} else {
			out->sb.st_atime = field;
		}
	}

	if (!(set_by_pax & PAX_CTIME)) {
		field = out->sb.st_mtime;

		if (version == ETV_PRE_POSIX &&
		    ((uint8_t)hdr->tail.gnu.ctime[0] == 0x80 ||
		     (uint8_t)hdr->tail.gnu.ctime[0] == 0xFF ||
		     isdigit(hdr->tail.gnu.ctime[0]))) {
			if (read_number(hdr->tail.gnu.ctime,
					sizeof(hdr->tail.gnu.atime), &field))
				return -1;
		}

		if (field & 0x8000000000000000UL) {
			field = ~field + 1;
			out->sb.st_ctime = -((int64_t)field);
		} else {
			out->sb.st_ctime = field;
		}
	}

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
	case TAR_TYPE_GNU_SPARSE:
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
	int ret, version;

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
		version = check_version(&hdr);

		if (version == ETV_UNKNOWN)
			goto fail_magic;

		if (!is_checksum_valid(&hdr))
			goto fail_chksum;

		switch (hdr.typeflag) {
		case TAR_TYPE_GNU_SLINK:
			if (read_number(hdr.size, sizeof(hdr.size), &pax_size))
				goto fail;
			free(out->link_target);
			out->link_target = record_to_memory(fd, pax_size);
			if (out->link_target == NULL)
				goto fail;
			set_by_pax |= PAX_SLINK_TARGET;
			continue;
		case TAR_TYPE_GNU_PATH:
			if (read_number(hdr.size, sizeof(hdr.size), &pax_size))
				goto fail;
			free(out->name);
			out->name = record_to_memory(fd, pax_size);
			if (out->name == NULL)
				goto fail;
			set_by_pax |= PAX_NAME;
			continue;
		case TAR_TYPE_PAX:
			clear_header(out);
			if (read_number(hdr.size, sizeof(hdr.size), &pax_size))
				goto fail;
			set_by_pax = 0;
			if (read_pax_header(fd, pax_size, &set_by_pax, out))
				goto fail;
			continue;
		case TAR_TYPE_GNU_SPARSE:
			free_sparse_list(out->sparse);
			out->sparse = read_gnu_old_sparse(fd, &hdr);
			if (out->sparse == NULL)
				goto fail;
			if (read_number(hdr.tail.gnu.realsize,
					sizeof(hdr.tail.gnu.realsize),
					&out->actual_size))
				goto fail;
			break;
		}
		break;
	}

	if (decode_header(&hdr, set_by_pax, out, version))
		goto fail;

	if (out->sparse != NULL) {
		out->sb.st_size = out->actual_size;
	} else {
		out->sb.st_size = out->record_size;
		out->actual_size = out->record_size;
	}
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
