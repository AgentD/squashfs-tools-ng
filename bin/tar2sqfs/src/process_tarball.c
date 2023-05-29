/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * process_tarball.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "tar2sqfs.h"

static int write_file(sqfs_writer_t *sqfs, dir_iterator_t *it,
		      const dir_entry_t *ent, tree_node_t *n)
{
	int flags = 0, ret = 0;
	ostream_t *out;
	istream_t *in;

	if (no_tail_pack && ent->size > cfg.block_size)
		flags |= SQFS_BLK_DONT_FRAGMENT;

	out = data_writer_ostream_create(ent->name, sqfs->data,
					 &(n->data.file.inode), flags);
	if (out == NULL)
		return -1;

	ret = it->open_file_ro(it, &in);
	if (ret != 0) {
		sqfs_drop(out);
		return ret;
	}

	do {
		ret = ostream_append_from_istream(out, in, cfg.block_size);
	} while (ret > 0);

	ostream_flush(out);
	sqfs_drop(out);
	sqfs_drop(in);
	return ret;
}

static int copy_xattr(sqfs_writer_t *sqfs, const char *filename,
		      tree_node_t *node, dir_iterator_t *it)
{
	dir_entry_xattr_t *xattr, *list;
	int ret;

	ret = it->read_xattr(it, &list);
	if (ret) {
		sqfs_perror(filename, "reading xattrs", ret);
		return -1;
	}

	ret = sqfs_xattr_writer_begin(sqfs->xwr, 0);
	if (ret) {
		sqfs_perror(filename, "beginning xattr block", ret);
		goto fail;
	}

	for (xattr = list; xattr != NULL; xattr = xattr->next) {
		if (sqfs_get_xattr_prefix_id(xattr->key) < 0) {
			fprintf(stderr, "%s: squashfs does not "
				"support xattr prefix of %s\n",
				dont_skip ? "ERROR" : "WARNING",
				xattr->key);

			if (dont_skip)
				goto fail;
			continue;
		}

		ret = sqfs_xattr_writer_add(sqfs->xwr, xattr->key, xattr->value,
					    xattr->value_len);
		if (ret) {
			sqfs_perror(filename, "storing xattr key-value pair",
				    ret);
			goto fail;
		}
	}

	ret = sqfs_xattr_writer_end(sqfs->xwr, &node->xattr_idx);
	if (ret) {
		sqfs_perror(filename, "completing xattr block", ret);
		goto fail;
	}

	dir_entry_xattr_list_free(list);
	return 0;
fail:
	dir_entry_xattr_list_free(list);
	return -1;
}

static int create_node_and_repack_data(sqfs_writer_t *sqfs, dir_iterator_t *it,
				       const dir_entry_t *ent, const char *link)
{
	tree_node_t *node;

	node = fstree_add_generic(&sqfs->fs, ent, link);
	if (node == NULL)
		goto fail_errno;

	if (!cfg.quiet) {
		if (ent->flags & DIR_ENTRY_FLAG_HARD_LINK) {
			printf("Hard link %s -> %s\n", ent->name, link);
		} else {
			printf("Packing %s\n", ent->name);
		}
	}

	if (!cfg.no_xattr) {
		if (copy_xattr(sqfs, ent->name, node, it))
			return -1;
	}

	if (S_ISREG(ent->mode)) {
		int ret = write_file(sqfs, it, ent, node);
		if (ret != 0) {
			sqfs_perror(ent->name, "packing data", ret);
			return -1;
		}
	}

	return 0;
fail_errno:
	perror(ent->name);
	return -1;
}

static int set_root_attribs(sqfs_writer_t *sqfs, dir_iterator_t *it,
			    const dir_entry_t *ent)
{
	if ((ent->flags & DIR_ENTRY_FLAG_HARD_LINK) || !S_ISDIR(ent->mode)) {
		fprintf(stderr, "'%s' is not a directory!\n", ent->name);
		return -1;
	}

	sqfs->fs.root->uid = ent->uid;
	sqfs->fs.root->gid = ent->gid;
	sqfs->fs.root->mode = ent->mode;

	if (keep_time)
		sqfs->fs.root->mod_time = ent->mtime;

	if (!cfg.no_xattr) {
		if (copy_xattr(sqfs, "/", sqfs->fs.root, it))
			return -1;
	}

	return 0;
}

int process_tarball(istream_t *input_file, sqfs_writer_t *sqfs)
{
	dir_iterator_t *it = tar_open_stream(input_file);
	size_t rootlen = root_becomes == NULL ? 0 : strlen(root_becomes);

	if (it == NULL) {
		fputs("Creating tar stream: out-of-memory\n", stderr);
		return -1;
	}

	for (;;) {
		bool skip = false, is_root = false, is_prefixed = true;
		dir_entry_t *ent = NULL;
		char *link = NULL;
		int ret;

		ret = it->next(it, &ent);
		if (ret > 0)
			break;
		if (ret < 0)
			goto fail;

		if (ent->mtime < 0)
			ent->mtime = 0;

		if ((sqfs_u64)ent->mtime > 0x0FFFFFFFFUL)
			ent->mtime = 0x0FFFFFFFFUL;

		if (S_ISLNK(ent->mode)) {
			ret = it->read_link(it, &link);
			if (ret != 0) {
				sqfs_perror(ent->name, "read link", ret);
				free(ent);
				goto fail;
			}
		}

		if (root_becomes != NULL) {
			if (strncmp(ent->name, root_becomes, rootlen) == 0) {
				if (ent->name[rootlen] == '\0') {
					is_root = true;
				} else if (ent->name[rootlen] != '/') {
					is_prefixed = false;
				}
			} else {
				is_prefixed = false;
			}

			if (!is_prefixed) {
				free(ent);
				free(link);
				continue;
			}

			if (!is_root) {
				memmove(ent->name, ent->name + rootlen + 1,
					strlen(ent->name + rootlen + 1) + 1);
			}

			if (link != NULL &&
			    ((ent->flags & DIR_ENTRY_FLAG_HARD_LINK) ||
			     !no_symlink_retarget)) {
				if (canonicalize_name(link) == 0 &&
				    !strncmp(link, root_becomes, rootlen) &&
				    link[rootlen] == '/') {
					memmove(link, link + rootlen,
						strlen(link + rootlen) + 1);
				}
			}
		} else if (ent->name[0] == '\0') {
			is_root = true;
		}

		if (!keep_time)
			ent->mtime = sqfs->fs.defaults.mtime;

		if (is_root) {
			ret = set_root_attribs(sqfs, it, ent);
		} else if (skip) {
			ret = dont_skip ? -1 : 0;
		} else {
			ret = create_node_and_repack_data(sqfs, it, ent, link);
		}

		free(ent);
		free(link);
		if (ret)
			goto fail;
	}

	sqfs_drop(it);
	return 0;
fail:
	sqfs_drop(it);
	return -1;
}
