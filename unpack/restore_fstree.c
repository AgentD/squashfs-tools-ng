/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * restore_fstree.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "rdsquashfs.h"

static int create_node(const sqfs_tree_node_t *n, int flags)
{
	const sqfs_tree_node_t *c;
	char *name;
	int fd;

	if (!(flags & UNPACK_QUIET)) {
		name = sqfs_tree_node_get_path(n);
		printf("creating %s\n", name);
		free(name);
	}

	switch (n->inode->base.mode & S_IFMT) {
	case S_IFDIR:
		if (mkdir((const char *)n->name, 0755) && errno != EEXIST) {
			fprintf(stderr, "mkdir %s: %s\n",
				n->name, strerror(errno));
			return -1;
		}

		if (pushd((const char *)n->name))
			return -1;

		for (c = n->children; c != NULL; c = c->next) {
			if (create_node(c, flags))
				return -1;
		}

		if (popd())
			return -1;
		break;
	case S_IFLNK:
		if (symlink(n->inode->slink_target, (const char *)n->name)) {
			fprintf(stderr, "ln -s %s %s: %s\n",
				n->inode->slink_target, n->name,
				strerror(errno));
			return -1;
		}
		break;
	case S_IFSOCK:
	case S_IFIFO:
		if (mknod((const char *)n->name,
			  (n->inode->base.mode & S_IFMT) | 0700, 0)) {
			fprintf(stderr, "creating %s: %s\n",
				n->name, strerror(errno));
			return -1;
		}
		break;
	case S_IFBLK:
	case S_IFCHR: {
		uint32_t devno;

		if (n->inode->base.type == SQFS_INODE_EXT_BDEV ||
		    n->inode->base.type == SQFS_INODE_EXT_CDEV) {
			devno = n->inode->data.dev_ext.devno;
		} else {
			devno = n->inode->data.dev.devno;
		}

		if (mknod((const char *)n->name, n->inode->base.mode & S_IFMT,
			  devno)) {
			fprintf(stderr, "creating device %s: %s\n",
				n->name, strerror(errno));
			return -1;
		}
		break;
	}
	case S_IFREG:
		fd = open((const char *)n->name, O_WRONLY | O_CREAT | O_EXCL,
			  0600);
		if (fd < 0) {
			fprintf(stderr, "creating %s: %s\n",
				n->name, strerror(errno));
			return -1;
		}

		close(fd);
		break;
	default:
		break;
	}

	return 0;
}

#ifdef HAVE_SYS_XATTR_H
static int set_xattr(sqfs_xattr_reader_t *xattr, const sqfs_tree_node_t *n)
{
	sqfs_xattr_value_t *value;
	sqfs_xattr_entry_t *key;
	sqfs_xattr_id_t desc;
	uint32_t index;
	size_t i;
	int ret;

	sqfs_inode_get_xattr_index(n->inode, &index);

	if (index == 0xFFFFFFFF)
		return 0;

	if (sqfs_xattr_reader_get_desc(xattr, index, &desc)) {
		fputs("Error resolving xattr index\n", stderr);
		return -1;
	}

	if (sqfs_xattr_reader_seek_kv(xattr, &desc)) {
		fputs("Error locating xattr key-value pairs\n", stderr);
		return -1;
	}

	for (i = 0; i < desc.count; ++i) {
		if (sqfs_xattr_reader_read_key(xattr, &key)) {
			fputs("Error reading xattr key\n", stderr);
			return -1;
		}

		if (sqfs_xattr_reader_read_value(xattr, key, &value)) {
			fputs("Error reading xattr value\n", stderr);
			free(key);
			return -1;
		}

		ret = lsetxattr((const char *)n->name, (const char *)key->key,
				value->value, value->size, 0);
		if (ret) {
			fprintf(stderr, "setting xattr '%s' on %s: %s\n",
				key->key, n->name, strerror(errno));
		}

		free(key);
		free(value);
		if (ret)
			return -1;
	}

	return 0;
}
#endif

static int set_attribs(sqfs_xattr_reader_t *xattr,
		       const sqfs_tree_node_t *n, int flags)
{
	const sqfs_tree_node_t *c;

	if (S_ISDIR(n->inode->base.mode)) {
		if (pushd((const char *)n->name))
			return -1;

		for (c = n->children; c != NULL; c = c->next) {
			if (set_attribs(xattr, c, flags))
				return -1;
		}

		if (popd())
			return -1;
	}

#ifdef HAVE_SYS_XATTR_H
	if ((flags & UNPACK_SET_XATTR) && xattr != NULL) {
		if (set_xattr(xattr, n))
			return -1;
	}
#endif

	if (flags & UNPACK_SET_TIMES) {
		struct timespec times[2];

		memset(times, 0, sizeof(times));
		times[0].tv_sec = n->inode->base.mod_time;
		times[1].tv_sec = n->inode->base.mod_time;

		if (utimensat(AT_FDCWD, (const char *)n->name, times,
			      AT_SYMLINK_NOFOLLOW)) {
			fprintf(stderr, "setting timestamp on %s: %s\n",
				n->name, strerror(errno));
			return -1;
		}
	}

	if (flags & UNPACK_CHOWN) {
		if (fchownat(AT_FDCWD, (const char *)n->name, n->uid, n->gid,
			     AT_SYMLINK_NOFOLLOW)) {
			fprintf(stderr, "chown %s: %s\n",
				n->name, strerror(errno));
			return -1;
		}
	}

	if (flags & UNPACK_CHMOD && !S_ISLNK(n->inode->base.mode)) {
		if (fchmodat(AT_FDCWD, (const char *)n->name,
			     n->inode->base.mode & ~S_IFMT, 0)) {
			fprintf(stderr, "chmod %s: %s\n",
				n->name, strerror(errno));
			return -1;
		}
	}
	return 0;
}

int restore_fstree(sqfs_tree_node_t *root, int flags)
{
	sqfs_tree_node_t *n, *old_parent;

	/* make sure fstree_get_path() stops at this node */
	old_parent = root->parent;
	root->parent = NULL;

	if (S_ISDIR(root->inode->base.mode)) {
		for (n = root->children; n != NULL; n = n->next) {
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

int update_tree_attribs(sqfs_xattr_reader_t *xattr,
			const sqfs_tree_node_t *root, int flags)
{
	const sqfs_tree_node_t *n;

	if ((flags & (UNPACK_CHOWN | UNPACK_CHMOD |
		      UNPACK_SET_TIMES | UNPACK_SET_XATTR)) == 0) {
		return 0;
	}

	if (S_ISDIR(root->inode->base.mode)) {
		for (n = root->children; n != NULL; n = n->next) {
			if (set_attribs(xattr, n, flags))
				return -1;
		}
	} else {
		if (set_attribs(xattr, root, flags))
			return -1;
	}

	return 0;
}
