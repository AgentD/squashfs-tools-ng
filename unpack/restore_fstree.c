/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"

#include "rdsquashfs.h"

static int create_node(tree_node_t *n, data_reader_t *data, int flags)
{
	tree_node_t *c;
	char *name;
	int fd;

	if (!(flags & UNPACK_QUIET)) {
		name = fstree_get_path(n);
		printf("unpacking %s\n", name);
		free(name);
	}

	switch (n->mode & S_IFMT) {
	case S_IFDIR:
		if (mkdir(n->name, 0755) && errno != EEXIST) {
			fprintf(stderr, "mkdir %s: %s\n",
				n->name, strerror(errno));
			return -1;
		}

		if (pushd(n->name))
			return -1;

		for (c = n->data.dir->children; c != NULL; c = c->next) {
			if (create_node(c, data, flags))
				return -1;
		}

		if (popd())
			return -1;
		break;
	case S_IFLNK:
		if (symlink(n->data.slink_target, n->name)) {
			fprintf(stderr, "ln -s %s %s: %s\n",
				n->data.slink_target, n->name,
				strerror(errno));
			return -1;
		}
		break;
	case S_IFSOCK:
	case S_IFIFO:
		if (mknod(n->name, (n->mode & S_IFMT) | 0700, 0)) {
			fprintf(stderr, "creating %s: %s\n",
				n->name, strerror(errno));
			return -1;
		}
		break;
	case S_IFBLK:
	case S_IFCHR:
		if (mknod(n->name, n->mode & S_IFMT, n->data.devno)) {
			fprintf(stderr, "creating device %s: %s\n",
				n->name, strerror(errno));
			return -1;
		}
		break;
	case S_IFREG:
		fd = open(n->name, O_WRONLY | O_CREAT | O_EXCL, 0600);
		if (fd < 0) {
			fprintf(stderr, "creating %s: %s\n",
				n->name, strerror(errno));
			return -1;
		}

		if (data_reader_dump_file(data, n->data.file, fd,
					  (flags & UNPACK_NO_SPARSE) == 0)) {
			close(fd);
			return -1;
		}

		close(fd);
		break;
	default:
		break;
	}

	if (flags & UNPACK_CHOWN) {
		if (fchownat(AT_FDCWD, n->name, n->uid, n->gid,
			     AT_SYMLINK_NOFOLLOW)) {
			fprintf(stderr, "chown %s: %s\n",
				n->name, strerror(errno));
			return -1;
		}
	}

	if (flags & UNPACK_CHMOD) {
		if (fchmodat(AT_FDCWD, n->name, n->mode,
			     AT_SYMLINK_NOFOLLOW)) {
			fprintf(stderr, "chmod %s: %s\n",
				n->name, strerror(errno));
			return -1;
		}
	}
	return 0;
}

int restore_fstree(const char *rootdir, tree_node_t *root,
		   data_reader_t *data, int flags)
{
	tree_node_t *n;

	if (rootdir != NULL) {
		if (mkdir_p(rootdir))
			return -1;

		if (pushd(rootdir))
			return -1;
	}

	if (S_ISDIR(root->mode)) {
		for (n = root->data.dir->children; n != NULL; n = n->next) {
			if (create_node(n, data, flags))
				return -1;
		}
	} else {
		if (create_node(root, data, flags))
			return -1;
	}

	return rootdir == NULL ? 0 : popd();
}
