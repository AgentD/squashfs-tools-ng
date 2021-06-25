/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * describe.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "rdsquashfs.h"

static int print_name(const sqfs_tree_node_t *n, bool dont_escape)
{
	char *start, *ptr, *name = sqfs_tree_node_get_path(n);

	if (name == NULL) {
		perror("Recovering file path of tree node");
		return -1;
	}

	if (canonicalize_name(name) != 0) {
		fprintf(stderr, "Error sanitizing file path '%s'\n", name);
		free(name);
		return -1;
	}

	if (dont_escape || (strchr(name, ' ') == NULL &&
			    strchr(name, '"') == NULL)) {
		fputs(name, stdout);
	} else {
		fputc('"', stdout);

		ptr = strchr(name, '"');

		if (ptr != NULL) {
			start = name;

			do {
				fwrite(start, 1, ptr - start, stdout);
				fputs("\\\"", stdout);
				start = ptr + 1;
				ptr = strchr(start, '"');
			} while (ptr != NULL);

			fputs(start, stdout);
		} else {
			fputs(name, stdout);
		}

		fputc('"', stdout);
	}

	free(name);
	return 0;
}

static void print_perm(const sqfs_tree_node_t *n)
{
	printf(" 0%o %u %u", (unsigned int)n->inode->base.mode & (~S_IFMT),
	       n->uid, n->gid);
}

static int print_simple(const char *type, const sqfs_tree_node_t *n,
			const char *extra)
{
	printf("%s ", type);
	if (print_name(n, false))
		return -1;
	print_perm(n);
	if (extra != NULL)
		printf(" %s", extra);
	fputc('\n', stdout);
	return 0;
}

int describe_tree(const sqfs_tree_node_t *root, const char *unpack_root)
{
	const sqfs_tree_node_t *n;

	if (!is_filename_sane((const char *)root->name, false)) {
		fprintf(stderr, "Encountered illegal file name '%s'\n",
			root->name);
		return -1;
	}

	switch (root->inode->base.mode & S_IFMT) {
	case S_IFSOCK:
		return print_simple("sock", root, NULL);
	case S_IFLNK:
		return print_simple("slink", root,
				    (const char *)root->inode->extra);
	case S_IFIFO:
		return print_simple("pipe", root, NULL);
	case S_IFREG:
		if (unpack_root == NULL)
			return print_simple("file", root, NULL);

		fputs("file ", stdout);
		if (print_name(root, false))
			return -1;
		print_perm(root);
		printf(" %s/", unpack_root);
		if (print_name(root, true))
			return -1;
		fputc('\n', stdout);
		break;
	case S_IFCHR:
	case S_IFBLK: {
		char buffer[32];
		sqfs_u32 devno;

		if (root->inode->base.type == SQFS_INODE_EXT_BDEV ||
		    root->inode->base.type == SQFS_INODE_EXT_CDEV) {
			devno = root->inode->data.dev_ext.devno;
		} else {
			devno = root->inode->data.dev.devno;
		}

		sprintf(buffer, "%c %u %u",
			S_ISCHR(root->inode->base.mode) ? 'c' : 'b',
			major(devno), minor(devno));
		return print_simple("nod", root, buffer);
	}
	case S_IFDIR:
		if (root->name[0] != '\0') {
			if (print_simple("dir", root, NULL))
				return -1;
		}

		for (n = root->children; n != NULL; n = n->next) {
			if (describe_tree(n, unpack_root))
				return -1;
		}
		break;
	default:
		break;
	}

	return 0;
}
