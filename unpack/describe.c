/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"

#include "rdsquashfs.h"

#include <sys/sysmacros.h>

static void print_name(tree_node_t *n)
{
	if (n->parent != NULL) {
		print_name(n->parent);
		fputc('/', stdout);
	}

	fputs(n->name, stdout);
}

static void print_perm(tree_node_t *n)
{
	printf(" 0%o %d %d", n->mode & (~S_IFMT), n->uid, n->gid);
}

static void print_simple(const char *type, tree_node_t *n, const char *extra)
{
	printf("%s ", type);
	print_name(n);
	print_perm(n);
	if (extra != NULL)
		printf(" %s", extra);
	fputc('\n', stdout);
}

void describe_tree(tree_node_t *root, const char *unpack_root)
{
	tree_node_t *n;

	switch (root->mode & S_IFMT) {
	case S_IFSOCK:
		print_simple("sock", root, NULL);
		break;
	case S_IFLNK:
		print_simple("slink", root, root->data.slink_target);
		break;
	case S_IFIFO:
		print_simple("pipe", root, NULL);
		break;
	case S_IFREG:
		if (unpack_root != NULL) {
			fputs("file ", stdout);
			print_name(root);
			print_perm(root);
			printf(" %s", unpack_root);
			print_name(root);
			fputc('\n', stdout);
		} else {
			print_simple("file", root, NULL);
		}
		break;
	case S_IFCHR:
	case S_IFBLK: {
		char buffer[32];
		sprintf(buffer, "%c %d %d", S_ISCHR(root->mode) ? 'c' : 'b',
		       major(root->data.devno), minor(root->data.devno));
		print_simple("nod", root, buffer);
		break;
	}
	case S_IFDIR:
		if (root->name[0] != '\0')
			print_simple("dir", root, NULL);

		for (n = root->data.dir->children; n != NULL; n = n->next)
			describe_tree(n, unpack_root);
		break;
	}
}
