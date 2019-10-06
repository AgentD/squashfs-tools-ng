/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * fstree_from_dir.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "mkfs.h"

static char *get_file_path(tree_node_t *n, const char *name)
{
	char *ptr, *new;
	int ret;

	if (n->parent == NULL) {
		ptr = strdup(name);
		if (ptr == NULL)
			goto fail;
		return ptr;
	}

	ptr = fstree_get_path(n);
	if (ptr == NULL)
		goto fail;

	ret = canonicalize_name(ptr);
	assert(ret == 0);

	new = realloc(ptr, strlen(ptr) + strlen(name) + 2);
	if (new == NULL)
		goto fail;

	ptr = new;
	strcat(ptr, "/");
	strcat(ptr, name);
	return ptr;
fail:
	perror("getting absolute file path");
	free(ptr);
	return NULL;
}

#ifdef HAVE_SYS_XATTR_H
static int populate_xattr(sqfs_xattr_writer_t *xwr, tree_node_t *node)
{
	char *key, *value = NULL, *buffer = NULL;
	ssize_t buflen, vallen, keylen;
	int ret;

	buflen = listxattr(node->name, NULL, 0);

	if (buflen < 0) {
		perror("listxattr");
		return -1;
	}

	if (buflen == 0)
		return 0;

	buffer = malloc(buflen);
	if (buffer == NULL) {
		perror("xattr name buffer");
		return -1;
	}

	buflen = listxattr(node->name, buffer, buflen);
	if (buflen == -1) {
		perror("listxattr");
		goto fail;
	}

	key = buffer;
	while (buflen > 0) {
		vallen = getxattr(node->name, key, NULL, 0);
		if (vallen == -1)
			goto fail;

		if (vallen > 0) {
			value = calloc(1, vallen);
			if (value == NULL) {
				perror("allocating xattr value buffer");
				goto fail;
			}

			vallen = getxattr(node->name, key, value, vallen);
			if (vallen == -1) {
				fprintf(stderr, "%s: getxattr: %s\n",
					node->name, strerror(errno));
				goto fail;
			}

			ret = sqfs_xattr_writer_add(xwr, key, value, vallen);
			if (ret) {
				sqfs_perror(node->name,
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

static int populate_dir(fstree_t *fs, tree_node_t *root, dev_t devstart,
			void *selinux_handle, sqfs_xattr_writer_t *xwr,
			unsigned int flags)
{
	char *extra = NULL, *path;
	struct dirent *ent;
	struct stat sb;
	tree_node_t *n;
	DIR *dir;
	int ret;

	dir = opendir(".");
	if (dir == NULL) {
		perror("opendir");
		return -1;
	}

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

		if (fstatat(AT_FDCWD, ent->d_name, &sb, AT_SYMLINK_NOFOLLOW)) {
			perror(ent->d_name);
			goto fail;
		}

		if ((flags & DIR_SCAN_ONE_FILESYSTEM) && sb.st_dev != devstart)
			continue;

		if (S_ISLNK(sb.st_mode)) {
			extra = calloc(1, sb.st_size + 1);
			if (extra == NULL)
				goto fail_rdlink;

			if (readlink(ent->d_name, extra, sb.st_size) < 0)
				goto fail_rdlink;

			extra[sb.st_size] = '\0';
		} else if (S_ISREG(sb.st_mode)) {
			extra = get_file_path(root, ent->d_name);
			if (extra == NULL)
				goto fail;
		}

		if (!(flags & DIR_SCAN_KEEP_TIME))
			sb.st_mtim = fs->defaults.st_mtim;

		n = fstree_mknode(root, ent->d_name, strlen(ent->d_name),
				  extra, &sb);
		if (n == NULL) {
			perror("creating tree node");
			goto fail;
		}

		if (sqfs_xattr_writer_begin(xwr)) {
			fputs("error recoding xattr key-value pairs\n", stderr);
			return -1;
		}

#ifdef HAVE_SYS_XATTR_H
		if (flags & DIR_SCAN_READ_XATTR) {
			if (populate_xattr(xwr, n))
				goto fail;
		}
#endif
		if (selinux_handle != NULL) {
			path = fstree_get_path(n);
			if (path == NULL) {
				perror("getting full path for "
				       "SELinux relabeling");
				goto fail;
			}

			if (selinux_relable_node(selinux_handle, xwr,
						 n, path)) {
				free(path);
				goto fail;
			}

			free(path);
		}

		ret = sqfs_xattr_writer_end(xwr, &n->xattr_idx);
		if (ret) {
			sqfs_perror(n->name,
				    "completing xattr key-value pairs", ret);
			goto fail;
		}

		free(extra);
		extra = NULL;
	}

	closedir(dir);

	for (n = root->data.dir.children; n != NULL; n = n->next) {
		if (S_ISDIR(n->mode)) {
			if (pushd(n->name))
				return -1;

			if (populate_dir(fs, n, devstart, selinux_handle,
					 xwr, flags)) {
				return -1;
			}

			if (popd())
				return -1;
		}
	}

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
	int ret;

	if (stat(path, &sb)) {
		perror(path);
		return -1;
	}

	if (pushd(path))
		return -1;

	ret = populate_dir(fs, fs->root, sb.st_dev, selinux_handle,
			   xwr, flags);

	if (popd())
		ret = -1;

	return ret;
}
