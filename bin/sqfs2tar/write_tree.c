/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * write_tree.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "sqfs2tar.h"

static sqfs_hard_link_t *links = NULL;
static unsigned int record_counter;

static sqfs_hard_link_t *find_hard_link(const char *name, sqfs_u32 inum)
{
	sqfs_hard_link_t *lnk = NULL;

	for (lnk = links; lnk != NULL; lnk = lnk->next) {
		if (lnk->inode_number == inum) {
			if (strcmp(name, lnk->target) == 0)
				lnk = NULL;
			break;
		}
	}

	return lnk;
}

static void inode_stat(const sqfs_tree_node_t *node, struct stat *sb)
{
	memset(sb, 0, sizeof(*sb));

	sb->st_mode = node->inode->base.mode;
	sb->st_uid = node->uid;
	sb->st_gid = node->gid;
	sb->st_mtime = node->inode->base.mod_time;

	switch (node->inode->base.type) {
	case SQFS_INODE_BDEV:
	case SQFS_INODE_CDEV:
		sb->st_rdev = node->inode->data.dev.devno;
		break;
	case SQFS_INODE_EXT_BDEV:
	case SQFS_INODE_EXT_CDEV:
		sb->st_rdev = node->inode->data.dev_ext.devno;
		break;
	case SQFS_INODE_SLINK:
		sb->st_size = node->inode->data.slink.target_size;
		break;
	case SQFS_INODE_EXT_SLINK:
		sb->st_size = node->inode->data.slink_ext.target_size;
		break;
	case SQFS_INODE_FILE:
		sb->st_size = node->inode->data.file.file_size;
		break;
	case SQFS_INODE_EXT_FILE:
		sb->st_size = node->inode->data.file_ext.file_size;
		break;
	case SQFS_INODE_DIR:
		sb->st_size = node->inode->data.dir.size;
		break;
	case SQFS_INODE_EXT_DIR:
		sb->st_size = node->inode->data.dir_ext.size;
		break;
	default:
		break;
	}
}

static int write_tree_dfs(const sqfs_tree_node_t *n)
{
	sqfs_hard_link_t *lnk = NULL;
	tar_xattr_t *xattr = NULL;
	char *name, *target;
	struct stat sb;
	size_t len;
	int ret;

	inode_stat(n, &sb);

	if (n->parent == NULL) {
		if (root_becomes == NULL)
			goto skip_hdr;

		len = strlen(root_becomes);
		name = malloc(len + 2);
		if (name == NULL) {
			perror("creating root directory");
			return -1;
		}

		memcpy(name, root_becomes, len);
		name[len] = '/';
		name[len + 1] = '\0';
	} else {
		if (!is_filename_sane((const char *)n->name)) {
			fprintf(stderr, "Found a file named '%s', skipping.\n",
				n->name);
			if (dont_skip) {
				fputs("Not allowed to skip files, aborting!\n",
				      stderr);
				return -1;
			}
			return 0;
		}

		ret = sqfs_tree_node_get_path(n, &name);
		if (ret != 0) {
			sqfs_perror(NULL, "resolving tree node path", ret);
			return -1;
		}

		if (canonicalize_name(name))
			goto out_skip;

		name = assemble_tar_path(name, S_ISDIR(sb.st_mode));
		if (name == NULL)
			return -1;

		lnk = find_hard_link(name, n->inode->base.inode_number);
		if (lnk != NULL) {
			ret = write_hard_link(out_file, &sb, name, lnk->target,
					      record_counter++);
			sqfs_free(name);
			return ret;
		}
	}

	if (!no_xattr) {
		if (get_xattrs(name, n->inode, &xattr)) {
			sqfs_free(name);
			return -1;
		}
	}

	target = S_ISLNK(sb.st_mode) ? (char *)n->inode->extra : NULL;
	ret = write_tar_header(out_file, &sb, name, target, xattr,
			       record_counter++);
	free_xattr_list(xattr);

	if (ret > 0)
		goto out_skip;

	if (ret < 0) {
		sqfs_free(name);
		return -1;
	}

	if (S_ISREG(sb.st_mode)) {
		if (sqfs_data_reader_dump(name, data, n->inode, out_file,
					  super.block_size)) {
			sqfs_free(name);
			return -1;
		}

		if (padd_file(out_file, sb.st_size)) {
			sqfs_free(name);
			return -1;
		}
	}

	sqfs_free(name);
skip_hdr:
	for (n = n->children; n != NULL; n = n->next) {
		if (write_tree_dfs(n))
			return -1;
	}
	return 0;
out_skip:
	if (dont_skip) {
		fputs("Not allowed to skip files, aborting!\n", stderr);
		ret = -1;
	} else {
		fprintf(stderr, "Skipping %s\n", name);
		ret = 0;
	}
	sqfs_free(name);
	return ret;
}

int write_tree(const sqfs_tree_node_t *n)
{
	sqfs_hard_link_t *lnk;
	int status = -1;

	if (!no_links) {
		int ret = sqfs_tree_find_hard_links(n, &links);
		if (ret) {
			sqfs_perror(NULL, "detecting hard links in "
				    "file system tree", ret);
			return -1;
		}

		for (lnk = links; lnk != NULL; lnk = lnk->next) {
			lnk->target = assemble_tar_path(lnk->target, false);

			if (lnk->target == NULL)
				goto out_links;
		}
	}

	status = write_tree_dfs(n);
out_links:
	while (links != NULL) {
		lnk = links;
		links = links->next;
		sqfs_free(lnk->target);
		free(lnk);
	}
	return status;
}
