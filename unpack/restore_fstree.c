/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "rdsquashfs.h"

static int create_node(int dirfd, tree_node_t *n, unsqfs_info_t *info)
{
	int fd;

	switch (n->mode & S_IFMT) {
	case S_IFDIR:
		if (mkdirat(dirfd, n->name, 0755) && errno != EEXIST) {
			fprintf(stderr, "mkdir %s: %s\n",
				n->name, strerror(errno));
			return -1;
		}

		fd = openat(dirfd, n->name, O_RDONLY | O_DIRECTORY);
		if (fd < 0) {
			fprintf(stderr, "open dir %s: %s\n",
				n->name, strerror(errno));
			return -1;
		}

		for (n = n->data.dir->children; n != NULL; n = n->next) {
			if (create_node(fd, n, info)) {
				close(fd);
				return -1;
			}
		}

		close(fd);
		break;
	case S_IFLNK:
		if (symlinkat(n->data.slink_target, dirfd, n->name)) {
			fprintf(stderr, "ln -s %s %s: %s\n",
				n->data.slink_target, n->name,
				strerror(errno));
			return -1;
		}
		break;
	case S_IFSOCK:
	case S_IFIFO:
		if (mknodat(dirfd, n->name, (n->mode & S_IFMT) | 0700, 0)) {
			fprintf(stderr, "creating %s: %s\n",
				n->name, strerror(errno));
			return -1;
		}
		break;
	case S_IFBLK:
	case S_IFCHR:
		if (mknodat(dirfd, n->name, (n->mode & S_IFMT),
			    n->data.devno)) {
			fprintf(stderr, "creating device %s: %s\n",
				n->name, strerror(errno));
			return -1;
		}
		break;
	case S_IFREG:
		fd = openat(dirfd, n->name, O_WRONLY | O_CREAT | O_EXCL, 0600);
		if (fd < 0) {
			fprintf(stderr, "creating %s: %s\n",
				n->name, strerror(errno));
			return -1;
		}

		if (extract_file(n->data.file, info, fd)) {
			close(fd);
			return -1;
		}

		close(fd);
		break;
	default:
		break;
	}

	if (info->flags & UNPACK_CHOWN) {
		if (fchownat(dirfd, n->name, n->uid, n->gid,
			     AT_SYMLINK_NOFOLLOW)) {
			fprintf(stderr, "chown %s: %s\n",
				n->name, strerror(errno));
			return -1;
		}
	}

	if (info->flags & UNPACK_CHMOD) {
		if (fchmodat(dirfd, n->name, n->mode,
			     AT_SYMLINK_NOFOLLOW)) {
			fprintf(stderr, "chmod %s: %s\n",
				n->name, strerror(errno));
			return -1;
		}
	}
	return 0;
}

int restore_fstree(const char *rootdir, tree_node_t *root, unsqfs_info_t *info)
{
	tree_node_t *n;
	int dirfd;

	if (mkdir_p(rootdir))
		return -1;

	dirfd = open(rootdir, O_RDONLY | O_DIRECTORY);
	if (dirfd < 0) {
		perror(rootdir);
		return -1;
	}

	if (S_ISDIR(root->mode)) {
		for (n = root->data.dir->children; n != NULL; n = n->next) {
			if (create_node(dirfd, n, info)) {
				close(dirfd);
				return -1;
			}
		}
		return 0;
	}

	if (create_node(dirfd, root, info)) {
		close(dirfd);
		return -1;
	}

	close(dirfd);
	return 0;
}
