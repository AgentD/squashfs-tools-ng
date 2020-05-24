/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * write_tree.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "sqfs2tar.h"

static unsigned int record_counter;

static int get_xattrs(const char *name, const sqfs_inode_generic_t *inode,
		      tar_xattr_t **out)
{
	tar_xattr_t *list = NULL, *ent;
	sqfs_xattr_value_t *value;
	sqfs_xattr_entry_t *key;
	sqfs_xattr_id_t desc;
	sqfs_u32 index;
	size_t i;
	int ret;

	if (xr == NULL)
		return 0;

	sqfs_inode_get_xattr_index(inode, &index);

	if (index == 0xFFFFFFFF)
		return 0;

	ret = sqfs_xattr_reader_get_desc(xr, index, &desc);
	if (ret) {
		sqfs_perror(name, "resolving xattr index", ret);
		return -1;
	}

	ret = sqfs_xattr_reader_seek_kv(xr, &desc);
	if (ret) {
		sqfs_perror(name, "locating xattr key-value pairs", ret);
		return -1;
	}

	for (i = 0; i < desc.count; ++i) {
		ret = sqfs_xattr_reader_read_key(xr, &key);
		if (ret) {
			sqfs_perror(name, "reading xattr key", ret);
			goto fail;
		}

		ret = sqfs_xattr_reader_read_value(xr, key, &value);
		if (ret) {
			sqfs_perror(name, "reading xattr value", ret);
			free(key);
			goto fail;
		}

		ent = calloc(1, sizeof(*ent) + strlen((const char *)key->key) +
			     value->size + 2);
		if (ent == NULL) {
			perror("creating xattr entry");
			free(key);
			free(value);
			goto fail;
		}

		ent->key = ent->data;
		strcpy(ent->key, (const char *)key->key);

		ent->value = (sqfs_u8 *)ent->key + strlen(ent->key) + 1;
		memcpy(ent->value, value->value, value->size + 1);

		ent->value_len = value->size;
		ent->next = list;
		list = ent;

		free(key);
		free(value);
	}

	*out = list;
	return 0;
fail:
	while (list != NULL) {
		ent = list;
		list = list->next;
		free(ent);
	}
	return -1;
}

int write_tree_dfs(const sqfs_tree_node_t *n)
{
	tar_xattr_t *xattr = NULL, *xit;
	sqfs_hard_link_t *lnk = NULL;
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
		if (!is_filename_sane((const char *)n->name, false)) {
			fprintf(stderr, "Found a file named '%s', skipping.\n",
				n->name);
			if (dont_skip) {
				fputs("Not allowed to skip files, aborting!\n",
				      stderr);
				return -1;
			}
			return 0;
		}

		name = sqfs_tree_node_get_path(n);
		if (name == NULL) {
			perror("resolving tree node path");
			return -1;
		}

		if (canonicalize_name(name))
			goto out_skip;

		for (lnk = links; lnk != NULL; lnk = lnk->next) {
			if (lnk->inode_number == n->inode->base.inode_number) {
				if (strcmp(name, lnk->target) == 0)
					lnk = NULL;
				break;
			}
		}

		name = assemble_tar_path(name, S_ISDIR(sb.st_mode));
		if (name == NULL)
			return -1;
	}

	if (lnk != NULL) {
		ret = write_hard_link(out_file, &sb, name, lnk->target,
				      record_counter++);
		free(name);
		return ret;
	}

	if (!no_xattr) {
		if (get_xattrs(name, n->inode, &xattr)) {
			free(name);
			return -1;
		}
	}

	target = S_ISLNK(sb.st_mode) ? (char *)n->inode->extra : NULL;
	ret = write_tar_header(out_file, &sb, name, target, xattr,
			       record_counter++);

	while (xattr != NULL) {
		xit = xattr;
		xattr = xattr->next;
		free(xit);
	}

	if (ret > 0)
		goto out_skip;

	if (ret < 0) {
		free(name);
		return -1;
	}

	if (S_ISREG(sb.st_mode)) {
		if (sqfs_data_reader_dump(name, data, n->inode, out_file,
					  super.block_size, false)) {
			free(name);
			return -1;
		}

		if (padd_file(out_file, sb.st_size)) {
			free(name);
			return -1;
		}
	}

	free(name);
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
	free(name);
	return ret;
}
