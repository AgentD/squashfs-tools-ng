/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * process_tarball.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "tar2sqfs.h"

static int write_file(sqfs_writer_t *sqfs, sqfs_dir_iterator_t *it,
		      const sqfs_dir_entry_t *ent, tree_node_t *n)
{
	int flags = 0, ret = 0;
	sqfs_ostream_t *out;
	sqfs_istream_t *in;

	if (no_tail_pack && ent->size > cfg.block_size)
		flags |= SQFS_BLK_DONT_FRAGMENT;

	ret = sqfs_block_processor_create_ostream(&out, ent->name, sqfs->data,
						  &(n->data.file.inode), flags);
	if (ret)
		return ret;

	ret = it->open_file_ro(it, &in);
	if (ret != 0) {
		sqfs_drop(out);
		return ret;
	}

	do {
		ret = sqfs_istream_splice(in, out, cfg.block_size);
	} while (ret > 0);

	if (ret == 0)
		ret = out->flush(out);

	sqfs_drop(out);
	sqfs_drop(in);
	return ret;
}

static int copy_xattr(sqfs_writer_t *sqfs, const char *filename,
		      tree_node_t *node, sqfs_dir_iterator_t *it)
{
	sqfs_xattr_t *xattr, *list;
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
		ret = sqfs_xattr_writer_add(sqfs->xwr, xattr);

		if (ret == SQFS_ERROR_UNSUPPORTED) {
			fprintf(stderr, "%s: squashfs does not "
				"support xattr prefix of %s\n",
				dont_skip ? "ERROR" : "WARNING",
				xattr->key);

			if (dont_skip)
				goto fail;
			continue;
		}

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

	sqfs_xattr_list_free(list);
	return 0;
fail:
	sqfs_xattr_list_free(list);
	return -1;
}

static int create_node_and_repack_data(sqfs_writer_t *sqfs,
				       sqfs_dir_iterator_t *it,
				       const sqfs_dir_entry_t *ent,
				       const char *link)
{
	tree_node_t *node;

	node = fstree_add_generic(&sqfs->fs, ent, link);
	if (node == NULL)
		goto fail_errno;

	if (!cfg.quiet) {
		if (ent->flags & SQFS_DIR_ENTRY_FLAG_HARD_LINK) {
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

static int set_root_attribs(sqfs_writer_t *sqfs, sqfs_dir_iterator_t *it,
			    const sqfs_dir_entry_t *ent)
{
	if ((ent->flags & SQFS_DIR_ENTRY_FLAG_HARD_LINK) || !S_ISDIR(ent->mode)) {
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

int process_tarball(sqfs_dir_iterator_t *it, sqfs_writer_t *sqfs)
{
	size_t rootlen = root_becomes == NULL ? 0 : strlen(root_becomes);

	for (;;) {
		bool is_root = false, is_prefixed = true;
		sqfs_dir_entry_t *ent = NULL;
		char *link = NULL;
		int ret;

		ret = it->next(it, &ent);
		if (ret > 0)
			break;
		if (ret < 0)
			return -1;

		if (ent->mtime < 0)
			ent->mtime = 0;

		if ((sqfs_u64)ent->mtime > 0x0FFFFFFFFUL)
			ent->mtime = 0x0FFFFFFFFUL;

		if (S_ISLNK(ent->mode)) {
			ret = it->read_link(it, &link);
			if (ret != 0) {
				sqfs_perror(ent->name, "read link", ret);
				free(ent);
				return -1;
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
			    ((ent->flags & SQFS_DIR_ENTRY_FLAG_HARD_LINK) ||
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
		} else {
			ret = create_node_and_repack_data(sqfs, it, ent, link);
		}

		free(ent);
		free(link);
		if (ret)
			return -1;
	}

	return 0;
}
