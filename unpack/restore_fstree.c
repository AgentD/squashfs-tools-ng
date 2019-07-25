/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "rdsquashfs.h"

static int create_node(tree_node_t *n, int flags)
{
	tree_node_t *c;
	char *name;
	int fd;

	if (!(flags & UNPACK_QUIET)) {
		name = fstree_get_path(n);
		printf("creating %s\n", name);
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
			if (create_node(c, flags))
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

		close(fd);

		if (n->parent != NULL) {
			n->data.file->input_file = fstree_get_path(n);
		} else {
			n->data.file->input_file = strdup(n->name);
		}

		if (n->data.file->input_file == NULL) {
			perror("restoring file path");
			return -1;
		}

		assert(canonicalize_name(n->data.file->input_file) == 0);
		break;
	default:
		break;
	}

	return 0;
}

static int set_attribs(tree_node_t *n, int flags)
{
	tree_node_t *c;

	if (S_ISDIR(n->mode)) {
		if (pushd(n->name))
			return -1;

		for (c = n->data.dir->children; c != NULL; c = c->next) {
			if (set_attribs(c, flags))
				return -1;
		}

		if (popd())
			return -1;
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

int restore_fstree(tree_node_t *root, int flags)
{
	tree_node_t *n, *old_parent;

	/* make sure fstree_get_path() stops at this node */
	old_parent = root->parent;
	root->parent = NULL;

	if (S_ISDIR(root->mode)) {
		for (n = root->data.dir->children; n != NULL; n = n->next) {
			if (create_node(n, flags))
				return -1;
		}
	} else {
		if (create_node(root, flags))
			return -1;
	}

	root->parent = old_parent;
	return 0;
}

int update_tree_attribs(tree_node_t *root, int flags)
{
	if ((flags & (UNPACK_CHOWN | UNPACK_CHMOD)) == 0)
		return 0;

	return set_attribs(root, flags);
}
