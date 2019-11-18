/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * fstree_from_dir.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "mkfs.h"

#ifdef HAVE_SYS_XATTR_H
static char *get_full_path(const char *prefix, tree_node_t *node)
{
	char *path = NULL, *new = NULL;
	size_t path_len, prefix_len;
	int ret;

	path = fstree_get_path(node);
	if (path == NULL)
		goto fail;

	ret = canonicalize_name(path);
	assert(ret == 0);

	path_len = strlen(path);
	prefix_len = strlen(prefix);

	while (prefix_len > 0 && prefix[prefix_len - 1] == '/')
		--prefix_len;

	if (prefix_len > 0) {
		new = realloc(path, path_len + prefix_len + 2);
		if (new == NULL)
			goto fail;

		path = new;

		memmove(path + prefix_len + 1, path, path_len + 1);
		memcpy(path, prefix, prefix_len);
		path[prefix_len] = '/';
	}

	return path;
fail:
	perror("getting full path for xattr scan");
	free(path);
	return NULL;
}

static int xattr_from_path(sqfs_xattr_writer_t *xwr, const char *path)
{
	char *key, *value = NULL, *buffer = NULL;
	ssize_t buflen, vallen, keylen;
	int ret;

	buflen = llistxattr(path, NULL, 0);
	if (buflen < 0) {
		fprintf(stderr, "llistxattr %s: %s", path, strerror(errno));
		return -1;
	}

	if (buflen == 0)
		return 0;

	buffer = malloc(buflen);
	if (buffer == NULL) {
		perror("xattr name buffer");
		return -1;
	}

	buflen = llistxattr(path, buffer, buflen);
	if (buflen == -1) {
		fprintf(stderr, "llistxattr %s: %s", path, strerror(errno));
		goto fail;
	}

	key = buffer;
	while (buflen > 0) {
		vallen = lgetxattr(path, key, NULL, 0);
		if (vallen == -1) {
			fprintf(stderr, "lgetxattr %s: %s",
				path, strerror(errno));
			goto fail;
		}

		if (vallen > 0) {
			value = calloc(1, vallen);
			if (value == NULL) {
				perror("allocating xattr value buffer");
				goto fail;
			}

			vallen = lgetxattr(path, key, value, vallen);
			if (vallen == -1) {
				fprintf(stderr, "lgetxattr %s: %s\n",
					path, strerror(errno));
				goto fail;
			}

			ret = sqfs_xattr_writer_add(xwr, key, value, vallen);
			if (ret) {
				sqfs_perror(path,
					    "storing xattr key-value pairs",
					    ret);
				goto fail;
			}

			free(value);
			value = NULL;
		}

		keylen = strlen(key) + 1;
		buflen -= keylen;
		key += keylen;
	}

	free(buffer);
	return 0;
fail:
	free(value);
	free(buffer);
	return -1;
}
#endif

static int xattr_xcan_dfs(const char *path_prefix, void *selinux_handle,
			  sqfs_xattr_writer_t *xwr, unsigned int flags,
			  tree_node_t *node)
{
	char *path;
	int ret;

	ret = sqfs_xattr_writer_begin(xwr);
	if (ret) {
		sqfs_perror(node->name, "recoding xattr key-value pairs\n",
			    ret);
		return -1;
	}

#ifdef HAVE_SYS_XATTR_H
	if (flags & DIR_SCAN_READ_XATTR) {
		path = get_full_path(path_prefix, node);
		if (path == NULL)
			return -1;

		ret = xattr_from_path(xwr, path);
		free(path);

		if (ret)
			return -1;
	}
#else
	(void)path_prefix;
#endif

	if (selinux_handle != NULL) {
		path = fstree_get_path(node);
		if (path == NULL) {
			perror("reconstructing absolute path");
			return -1;
		}

		ret = selinux_relable_node(selinux_handle, xwr, node, path);
		free(path);

		if (ret)
			return -1;
	}

	if (sqfs_xattr_writer_end(xwr, &node->xattr_idx)) {
		sqfs_perror(node->name, "completing xattr key-value pairs",
			    ret);
		return -1;
	}

	if (S_ISDIR(node->mode)) {
		node = node->data.dir.children;

		while (node != NULL) {
			if (xattr_xcan_dfs(path_prefix, selinux_handle, xwr,
					   flags, node)) {
				return -1;
			}

			node = node->next;
		}
	}

	return 0;
}

static int populate_dir(int dir_fd, fstree_t *fs, tree_node_t *root,
			dev_t devstart, unsigned int flags)
{
	char *extra = NULL;
	struct dirent *ent;
	struct stat sb;
	tree_node_t *n;
	int childfd;
	DIR *dir;

	dir = fdopendir(dir_fd);
	if (dir == NULL) {
		perror("fdopendir");
		close(dir_fd);
		return -1;
	}

	/* XXX: fdopendir can dup and close dir_fd internally
	   and still be compliant with the spec. */
	dir_fd = dirfd(dir);

	for (;;) {
		errno = 0;
		ent = readdir(dir);

		if (ent == NULL) {
			if (errno) {
				perror("readdir");
				goto fail;
			}
			break;
		}

		if (!strcmp(ent->d_name, "..") || !strcmp(ent->d_name, "."))
			continue;

		if (fstatat(dir_fd, ent->d_name, &sb, AT_SYMLINK_NOFOLLOW)) {
			perror(ent->d_name);
			goto fail;
		}

		if ((flags & DIR_SCAN_ONE_FILESYSTEM) && sb.st_dev != devstart)
			continue;

		if (S_ISLNK(sb.st_mode)) {
			extra = calloc(1, sb.st_size + 1);
			if (extra == NULL)
				goto fail_rdlink;

			if (readlinkat(dir_fd, ent->d_name,
				       extra, sb.st_size) < 0) {
				goto fail_rdlink;
			}

			extra[sb.st_size] = '\0';
		}

		if (!(flags & DIR_SCAN_KEEP_TIME))
			sb.st_mtim = fs->defaults.st_mtim;

		n = fstree_mknode(root, ent->d_name, strlen(ent->d_name),
				  extra, &sb);
		if (n == NULL) {
			perror("creating tree node");
			goto fail;
		}

		free(extra);
		extra = NULL;

		if (S_ISDIR(n->mode)) {
			childfd = openat(dir_fd, n->name, O_DIRECTORY |
					 O_RDONLY | O_CLOEXEC);
			if (childfd < 0) {
				perror(n->name);
				goto fail;
			}

			if (populate_dir(childfd, fs, n, devstart, flags))
				goto fail;
		}
	}

	closedir(dir);
	return 0;
fail_rdlink:
	perror("readlink");
fail:
	closedir(dir);
	free(extra);
	return -1;
}

int fstree_from_dir(fstree_t *fs, const char *path, void *selinux_handle,
		    sqfs_xattr_writer_t *xwr, unsigned int flags)
{
	struct stat sb;
	int fd;

	fd = open(path, O_DIRECTORY | O_RDONLY | O_CLOEXEC);
	if (fd < 0) {
		perror(path);
		return -1;
	}

	if (fstat(fd, &sb)) {
		perror(path);
		close(fd);
		return -1;
	}

	if (populate_dir(fd, fs, fs->root, sb.st_dev, flags))
		return -1;

	if (xwr != NULL && (selinux_handle != NULL ||
			    (flags & DIR_SCAN_READ_XATTR))) {
		if (xattr_xcan_dfs(path, selinux_handle, xwr, flags, fs->root))
			return -1;
	}

	return 0;
}
