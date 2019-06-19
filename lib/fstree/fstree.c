/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "fstree.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

tree_node_t *fstree_mknode(fstree_t *fs, tree_node_t *parent, const char *name,
			   size_t name_len, const char *extra,
			   const struct stat *sb)
{
	size_t size = sizeof(tree_node_t);
	tree_node_t *n;
	char *ptr;

	switch (sb->st_mode & S_IFMT) {
	case S_IFLNK:
		size += strlen(extra) + 1;
		break;
	case S_IFDIR:
		size += sizeof(*n->data.dir);
		break;
	case S_IFREG:
		size += sizeof(*n->data.file);
		size += (sb->st_size / fs->block_size) * sizeof(uint32_t);
		if (extra != NULL)
			size += strlen(extra) + 1;
		break;
	}

	n = calloc(1, size + name_len + 1);
	if (n == NULL)
		return NULL;

	if (parent != NULL) {
		n->next = parent->data.dir->children;
		parent->data.dir->children = n;
		n->parent = parent;
	}

	n->uid = sb->st_uid;
	n->gid = sb->st_gid;
	n->mode = sb->st_mode;

	switch (sb->st_mode & S_IFMT) {
	case S_IFDIR:
		n->data.dir = (dir_info_t *)n->payload;
		break;
	case S_IFREG:
		n->data.file = (file_info_t *)n->payload;
		n->data.file->size = sb->st_size;
		if (extra == NULL)
			break;

		ptr = (char *)n->data.file->blocksizes;
		ptr += (sb->st_size / fs->block_size) * sizeof(uint32_t);
		n->data.file->input_file = ptr;
		strcpy(n->data.file->input_file, extra);
		break;
	case S_IFLNK:
		n->data.slink_target = (char *)n->payload;
		strcpy(n->data.slink_target, extra);
		break;
	case S_IFBLK:
	case S_IFCHR:
		n->data.devno = sb->st_rdev;
		break;
	}

	n->name = (char *)n + size;
	memcpy(n->name, name, name_len);
	return n;
}

static void free_recursive(tree_node_t *n)
{
	tree_node_t *it;

	if (S_ISDIR(n->mode)) {
		while (n->data.dir->children != NULL) {
			it = n->data.dir->children;
			n->data.dir->children = it->next;

			free_recursive(it);
		}
	}

	free(n);
}

static tree_node_t *child_by_name(tree_node_t *root, const char *name,
				  size_t len)
{
	tree_node_t *n = root->data.dir->children;

	while (n != NULL) {
		if (strncmp(n->name, name, len) == 0 && n->name[len] == '\0')
			break;

		n = n->next;
	}

	return n;
}

static tree_node_t *get_parent_node(fstree_t *fs, tree_node_t *root,
				    const char *path)
{
	const char *end;
	tree_node_t *n;

	for (;;) {
		if (!S_ISDIR(root->mode)) {
			errno = ENOTDIR;
			return NULL;
		}

		end = strchr(path, '/');
		if (end == NULL)
			break;

		n = child_by_name(root, path, end - path);

		if (n == NULL) {
			n = fstree_mknode(fs, root, path, end - path, NULL,
					  &fs->defaults);
			if (n == NULL)
				return NULL;

			n->data.dir->created_implicitly = true;
		}

		root = n;
		path = end + 1;
	}

	return root;
}

tree_node_t *fstree_add_generic(fstree_t *fs, const char *path,
				const struct stat *sb, const char *extra)
{
	tree_node_t *child, *parent;
	const char *name;

	parent = get_parent_node(fs, fs->root, path);
	if (parent == NULL)
		return NULL;

	name = strrchr(path, '/');
	name = (name == NULL ? path : (name + 1));

	child = child_by_name(parent, name, strlen(name));
	if (child != NULL) {
		if (!S_ISDIR(child->mode) || !S_ISDIR(sb->st_mode) ||
		    !child->data.dir->created_implicitly) {
			errno = EEXIST;
			return NULL;
		}

		child->data.dir->created_implicitly = false;
		return child;
	}

	return fstree_mknode(fs, parent, name, strlen(name), extra, sb);
}

int fstree_add_xattr(fstree_t *fs, tree_node_t *node,
		     const char *key, const char *value)
{
	tree_xattr_t *xattr, *prev, *it;
	size_t key_idx, value_idx;

	if (str_table_get_index(&fs->xattr_keys, key, &key_idx))
		return -1;

	if (str_table_get_index(&fs->xattr_values, value, &value_idx))
		return -1;

	if (sizeof(size_t) > sizeof(uint32_t)) {
		if (key_idx > 0xFFFFFFFFUL) {
			fputs("Too many unique xattr keys\n", stderr);
			return -1;
		}

		if (value_idx > 0xFFFFFFFFUL) {
			fputs("Too many unique xattr values\n", stderr);
			return -1;
		}
	}

	if (node->xattr == NULL) {
		xattr = calloc(1, sizeof(*xattr) + sizeof(uint64_t) * 4);
		if (xattr == NULL) {
			perror("adding extended attributes");
			return -1;
		}

		xattr->max_attr = 4;
		xattr->owner = node;

		xattr->next = fs->xattr;
		fs->xattr = xattr;

		node->xattr = xattr;
	} else {
		xattr = node->xattr;

		if (xattr->max_attr == xattr->num_attr) {
			prev = NULL;
			it = fs->xattr;

			while (it != xattr) {
				prev = it;
				it = it->next;
			}

			if (prev == NULL) {
				fs->xattr = xattr->next;
			} else {
				prev->next = xattr->next;
			}

			node->xattr = NULL;

			it = realloc(xattr, sizeof(*xattr) +
				     sizeof(uint64_t) * xattr->max_attr * 2);

			if (it == NULL) {
				perror("adding extended attributes");
				free(xattr);
				return -1;
			}

			xattr = it;
			xattr->max_attr *= 2;

			node->xattr = xattr;
			xattr->next = fs->xattr;
			fs->xattr = xattr;
		}
	}

	xattr->ref[xattr->num_attr]  = (uint64_t)key_idx << 32;
	xattr->ref[xattr->num_attr] |= (uint64_t)value_idx;
	xattr->num_attr += 1;
	return 0;
}

static int cmp_u64(const void *lhs, const void *rhs)
{
	uint64_t l = *((uint64_t *)lhs), r = *((uint64_t *)rhs);

	return l < r ? -1 : (l > r ? 1 : 0);
}

void fstree_xattr_reindex(fstree_t *fs)
{
	tree_xattr_t *it;
	size_t index = 0;

	for (it = fs->xattr; it != NULL; it = it->next)
		it->index = index++;
}

void fstree_xattr_deduplicate(fstree_t *fs)
{
	tree_xattr_t *it, *it1, *prev;
	int diff;

	for (it = fs->xattr; it != NULL; it = it->next)
		qsort(it->ref, it->num_attr, sizeof(it->ref[0]), cmp_u64);

	prev = NULL;
	it = fs->xattr;

	while (it != NULL) {
		for (it1 = fs->xattr; it1 != it; it1 = it1->next) {
			if (it1->num_attr != it->num_attr)
				continue;

			diff = memcmp(it1->ref, it->ref,
				      it->num_attr * sizeof(it->ref[0]));
			if (diff == 0)
				break;
		}

		if (it1 != it) {
			prev->next = it->next;
			it->owner->xattr = it1;

			free(it);
			it = prev->next;
		} else {
			prev = it;
			it = it->next;
		}
	}

	fstree_xattr_reindex(fs);
}

int fstree_init(fstree_t *fs, size_t block_size, uint32_t mtime,
		uint16_t default_mode, uint32_t default_uid,
		uint32_t default_gid)
{
	memset(fs, 0, sizeof(*fs));

	fs->defaults.st_uid = default_uid;
	fs->defaults.st_gid = default_gid;
	fs->defaults.st_mode = S_IFDIR | (default_mode & 07777);
	fs->defaults.st_mtime = mtime;
	fs->defaults.st_ctime = mtime;
	fs->defaults.st_atime = mtime;
	fs->defaults.st_blksize = block_size;
	fs->block_size = block_size;

	if (str_table_init(&fs->xattr_keys, FSTREE_XATTR_KEY_BUCKETS))
		return -1;

	if (str_table_init(&fs->xattr_values, FSTREE_XATTR_VALUE_BUCKETS)) {
		str_table_cleanup(&fs->xattr_keys);
		return -1;
	}

	fs->root = fstree_mknode(fs, NULL, "", 0, NULL, &fs->defaults);

	if (fs->root == NULL) {
		perror("initializing file system tree");
		str_table_cleanup(&fs->xattr_values);
		str_table_cleanup(&fs->xattr_keys);
		return -1;
	}

	return 0;
}

void fstree_cleanup(fstree_t *fs)
{
	tree_xattr_t *xattr;

	while (fs->xattr != NULL) {
		xattr = fs->xattr;
		fs->xattr = xattr->next;
		free(xattr);
	}

	str_table_cleanup(&fs->xattr_keys);
	str_table_cleanup(&fs->xattr_values);
	free_recursive(fs->root);
	free(fs->inode_table);
	memset(fs, 0, sizeof(*fs));
}
