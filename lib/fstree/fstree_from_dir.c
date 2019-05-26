/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "fstree.h"
#include "util.h"

#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>

static size_t path_size(tree_node_t *n)
{
	size_t size = 0;

	while (n != NULL) {
		size += strlen(n->name) + 1;
		n = n->parent;
	}

	return size;
}

static char *print_path(char *ptr, tree_node_t *n)
{
	if (n->parent != NULL) {
		ptr = print_path(ptr, n->parent);
		*(ptr++) = '/';
		strcpy(ptr, n->name);
		return ptr + strlen(ptr);
	}

	return ptr;
}

static int populate_dir(tree_node_t *root, int fd, size_t blocksize,
			const char *rootdir)
{
	size_t size, blockcount;
	struct dirent *ent;
	struct stat sb;
	tree_node_t *n;
	ssize_t ret;
	void *ptr;
	DIR *dir;
	int cfd;

	dir = fdopendir(fd);
	if (dir == NULL) {
		perror("fdopendir");
		close(fd);
		return -1;
	}

	for (;;) {
		n = NULL;
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

		if (fstatat(fd, ent->d_name, &sb, AT_SYMLINK_NOFOLLOW)) {
			perror(ent->d_name);
			goto fail;
		}

		size = sizeof(tree_node_t) + strlen(ent->d_name) + 1;

		switch (sb.st_mode & S_IFMT) {
		case S_IFLNK:
			size += sb.st_size + 1;
			break;
		case S_IFREG:
			blockcount = sb.st_size / blocksize + 1;

			size += sizeof(file_info_t);
			size += sizeof(uint32_t) * blockcount;

			size += path_size(root) + strlen(ent->d_name) + 1;
			size += strlen(rootdir) + 2;
			break;
		case S_IFDIR:
			size += sizeof(dir_info_t);
			break;
		}

		n = calloc(1, size);
		if (n == NULL) {
			perror("allocating tree node");
			goto fail;
		}

		n->uid = sb.st_uid;
		n->gid = sb.st_gid;
		n->mode = sb.st_mode;
		n->parent = root;

		ptr = n->payload;

		switch (sb.st_mode & S_IFMT) {
		case S_IFLNK:
			ret = readlinkat(fd, ent->d_name, ptr, sb.st_size);
			if (ret < 0) {
				perror("readlink");
				goto fail;
			}

			n->data.slink_target = ptr;
			ptr = (char *)ptr + strlen(ptr) + 1;
			break;
		case S_IFBLK:
		case S_IFCHR:
			n->data.devno = sb.st_rdev;
			break;
		case S_IFDIR:
			n->data.dir = ptr;
			ptr = (char *)ptr + sizeof(dir_info_t);
			break;
		case S_IFREG:
			n->data.file = ptr;
			ptr = (char *)ptr + sizeof(file_info_t);
			ptr = (char *)ptr + sizeof(uint32_t) * blockcount;

			n->data.file->input_file = ptr;
			n->data.file->size = sb.st_size;

			strcpy(ptr, rootdir);
			ptr = (char *)ptr + strlen(ptr);
			ptr = print_path(ptr, root);

			*((char*)ptr) = '/';
			strcpy((char *)ptr + 1, ent->d_name);
			ptr = (char *)ptr + strlen(ptr) + 1;
			break;
		}

		n->name = ptr;
		strcpy(ptr, ent->d_name);

		n->next = root->data.dir->children;
		root->data.dir->children = n;
	}

	for (n = root->data.dir->children; n != NULL; n = n->next) {
		if (S_ISDIR(n->mode)) {
			cfd = openat(fd, n->name, O_RDONLY | O_DIRECTORY);
			if (cfd < 0) {
				perror(n->name);
				goto fail_dir;
			}

			if (populate_dir(n, cfd, blocksize, rootdir)) {
				close(cfd);
				goto fail_dir;
			}
		}
	}

	closedir(dir);
	return 0;
fail:
	free(n);
fail_dir:
	closedir(dir);
	return -1;
}

int fstree_from_dir(fstree_t *fs, const char *path)
{
	int fd = open(path, O_RDONLY | O_DIRECTORY);

	if (fd < 0) {
		perror(path);
		return -1;
	}

	return populate_dir(fs->root, fd, fs->block_size, path);
}
