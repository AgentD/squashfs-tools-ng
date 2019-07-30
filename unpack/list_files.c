/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * list_files.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "rdsquashfs.h"

static void mode_to_str(uint16_t mode, char *p)
{
	switch (mode & S_IFMT) {
	case S_IFDIR:  *(p++) = 'd'; break;
	case S_IFCHR:  *(p++) = 'c'; break;
	case S_IFBLK:  *(p++) = 'b'; break;
	case S_IFREG:  *(p++) = '-'; break;
	case S_IFLNK:  *(p++) = 'l'; break;
	case S_IFSOCK: *(p++) = 's'; break;
	case S_IFIFO:  *(p++) = 'p'; break;
	default:       *(p++) = '?'; break;
	}

	*(p++) = (mode & S_IRUSR) ? 'r' : '-';
	*(p++) = (mode & S_IWUSR) ? 'w' : '-';

	switch (mode & (S_IXUSR | S_ISUID)) {
	case S_IXUSR | S_ISUID: *(p++) = 's'; break;
	case S_IXUSR:           *(p++) = 'x'; break;
	case S_ISUID:           *(p++) = 'S'; break;
	default:                *(p++) = '-'; break;
	}

	*(p++) = (mode & S_IRGRP) ? 'r' : '-';
	*(p++) = (mode & S_IWGRP) ? 'w' : '-';

	switch (mode & (S_IXGRP | S_ISGID)) {
	case S_IXGRP | S_ISGID: *(p++) = 's'; break;
	case S_IXGRP:           *(p++) = 'x'; break;
	case S_ISGID:           *(p++) = 'S'; break;
	case 0:                 *(p++) = '-'; break;
	}

	*(p++) = (mode & S_IROTH) ? 'r' : '-';
	*(p++) = (mode & S_IWOTH) ? 'w' : '-';

	switch (mode & (S_IXOTH | S_ISVTX)) {
	case S_IXOTH | S_ISVTX: *(p++) = 't'; break;
	case S_IXOTH:           *(p++) = 'x'; break;
	case S_ISVTX:           *(p++) = 'T'; break;
	case 0:                 *(p++) = '-'; break;
	}

	*p = '\0';
}

static int count_int_chars(unsigned int i)
{
	int count = 1;

	while (i > 10) {
		++count;
		i /= 10;
	}

	return count;
}

static void print_size(uint64_t size, char *buffer)
{
	static const char *suffices = "kMGTPEZY";
	int suffix = -1;

	while (size > 1024) {
		++suffix;
		size /= 1024;
	}

	if (suffix >= 0) {
		sprintf(buffer, "%u%c", (unsigned int)size, suffices[suffix]);
	} else {
		sprintf(buffer, "%u", (unsigned int)size);
	}
}

static void print_node_size(tree_node_t *n, char *buffer)
{
	switch (n->mode & S_IFMT) {
	case S_IFLNK:
		print_size(strlen(n->data.slink_target), buffer);
		break;
	case S_IFREG:
		print_size(n->data.file->size, buffer);
		break;
	case S_IFDIR:
		print_size(n->data.dir->size, buffer);
		break;
	case S_IFBLK:
	case S_IFCHR:
		sprintf(buffer, "%u:%u", major(n->data.devno),
			minor(n->data.devno));
		break;
	default:
		buffer[0] = '0';
		buffer[1] = '\0';
		break;
	}
}

void list_files(tree_node_t *node)
{
	int i, max_uid_chars = 0, max_gid_chars = 0, max_sz_chars = 0;
	char modestr[12], sizestr[32];
	tree_node_t *n;

	if (S_ISDIR(node->mode)) {
		for (n = node->data.dir->children; n != NULL; n = n->next) {
			i = count_int_chars(n->uid);
			max_uid_chars = i > max_uid_chars ? i : max_uid_chars;

			i = count_int_chars(n->gid);
			max_gid_chars = i > max_gid_chars ? i : max_gid_chars;

			print_node_size(n, sizestr);
			i = strlen(sizestr);
			max_sz_chars = i > max_sz_chars ? i : max_sz_chars;
		}

		for (n = node->data.dir->children; n != NULL; n = n->next) {
			mode_to_str(n->mode, modestr);
			print_node_size(n, sizestr);

			printf("%s %*u/%-*u %*s %s", modestr,
			       max_uid_chars, n->uid,
			       max_gid_chars, n->gid,
			       max_sz_chars, sizestr,
			       n->name);

			if (S_ISLNK(n->mode)) {
				printf(" -> %s\n", n->data.slink_target);
			} else {
				fputc('\n', stdout);
			}
		}
	} else {
		mode_to_str(node->mode, modestr);
		print_node_size(node, sizestr);

		printf("%s %u/%u %s %s", modestr,
		       node->uid, node->gid, sizestr, node->name);

		if (S_ISLNK(node->mode)) {
			printf(" -> %s\n", node->data.slink_target);
		} else {
			fputc('\n', stdout);
		}
	}
}
