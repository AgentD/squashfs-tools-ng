/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * dirscan_xattr.c
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
			  sqfs_xattr_writer_t *xwr, bool scan_xattr,
			  tree_node_t *node)
{
	char *path;
	int ret;

	ret = sqfs_xattr_writer_begin(xwr, 0);
	if (ret) {
		sqfs_perror(node->name, "recoding xattr key-value pairs\n",
			    ret);
		return -1;
	}

#ifdef HAVE_SYS_XATTR_H
	if (scan_xattr) {
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
					   scan_xattr, node)) {
				return -1;
			}

			node = node->next;
		}
	}

	return 0;
}

int xattrs_from_dir(fstree_t *fs, const char *path, void *selinux_handle,
		    sqfs_xattr_writer_t *xwr, bool scan_xattr)
{
	if (xwr == NULL)
		return 0;

	if (selinux_handle == NULL && !scan_xattr)
		return 0;

	return xattr_xcan_dfs(path, selinux_handle, xwr, scan_xattr, fs->root);
}
