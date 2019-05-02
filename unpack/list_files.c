/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "unsquashfs.h"

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

void list_files(tree_node_t *node)
{
	int i, max_uid_chars = 0, max_gid_chars = 0;
	char modestr[12];
	tree_node_t *n;

	if (S_ISDIR(node->mode)) {
		for (n = node->data.dir->children; n != NULL; n = n->next) {
			i = count_int_chars(n->uid);
			max_uid_chars = i > max_uid_chars ? i : max_uid_chars;

			i = count_int_chars(n->gid);
			max_gid_chars = i > max_gid_chars ? i : max_gid_chars;
		}

		for (n = node->data.dir->children; n != NULL; n = n->next) {
			mode_to_str(n->mode, modestr);

			printf("%s %*u/%-*u %s\n", modestr,
			       max_uid_chars, n->uid,
			       max_gid_chars, n->gid, n->name);
		}
	} else {
		mode_to_str(node->mode, modestr);

		printf("%s %u/%u %s\n", modestr,
		       node->uid, node->gid, node->name);
	}
}
