/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "fstree.h"
#include "util.h"

#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>

static char *get_file_path(tree_node_t *n, const char *name)
{
	char *ptr, *new;

	if (n->parent == NULL) {
		ptr = strdup(name);
		if (ptr == NULL)
			goto fail;
		return ptr;
	}

	ptr = fstree_get_path(n);
	if (ptr == NULL)
		goto fail;

	assert(canonicalize_name(ptr) == 0);

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

static int populate_dir(fstree_t *fs, tree_node_t *root)
{
	char *extra = NULL;
	struct dirent *ent;
	struct stat sb;
	tree_node_t *n;
	DIR *dir;

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

		n = fstree_mknode(fs, root, ent->d_name, strlen(ent->d_name),
				  extra, &sb);
		if (n == NULL) {
			perror("creating tree node");
			goto fail;
		}

		free(extra);
		extra = NULL;
	}

	closedir(dir);

	for (n = root->data.dir->children; n != NULL; n = n->next) {
		if (S_ISDIR(n->mode)) {
			if (pushd(n->name))
				return -1;

			if (populate_dir(fs, n))
				return -1;

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

int fstree_from_dir(fstree_t *fs, const char *path)
{
	int ret;

	if (pushd(path))
		return -1;

	ret = populate_dir(fs, fs->root);

	if (popd())
		ret = -1;

	return ret;
}
