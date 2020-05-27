/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * process_tarball.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "tar2sqfs.h"

static int write_file(FILE *input_file, sqfs_writer_t *sqfs,
		      const tar_header_decoded_t *hdr,
		      file_info_t *fi, sqfs_u64 filesize)
{
	sqfs_file_t *file;
	int flags;
	int ret;

	file = sqfs_get_stdin_file(input_file, hdr->sparse, filesize);
	if (file == NULL) {
		perror("packing files");
		return -1;
	}

	flags = 0;
	if (no_tail_pack && filesize > cfg.block_size)
		flags |= SQFS_BLK_DONT_FRAGMENT;

	ret = write_data_from_file(hdr->name, sqfs->data,
				   (sqfs_inode_generic_t **)&fi->user_ptr,
				   file, flags);
	sqfs_destroy(file);

	if (ret)
		return -1;

	return skip_padding(input_file, hdr->sparse == NULL ?
			    filesize : hdr->record_size);
}

static int copy_xattr(sqfs_writer_t *sqfs, tree_node_t *node,
		      const tar_header_decoded_t *hdr)
{
	tar_xattr_t *xattr;
	int ret;

	ret = sqfs_xattr_writer_begin(sqfs->xwr);
	if (ret) {
		sqfs_perror(hdr->name, "beginning xattr block", ret);
		return -1;
	}

	for (xattr = hdr->xattr; xattr != NULL; xattr = xattr->next) {
		if (sqfs_get_xattr_prefix_id(xattr->key) < 0) {
			fprintf(stderr, "%s: squashfs does not "
				"support xattr prefix of %s\n",
				dont_skip ? "ERROR" : "WARNING",
				xattr->key);

			if (dont_skip)
				return -1;
			continue;
		}

		ret = sqfs_xattr_writer_add(sqfs->xwr, xattr->key, xattr->value,
					    xattr->value_len);
		if (ret) {
			sqfs_perror(hdr->name, "storing xattr key-value pair",
				    ret);
			return -1;
		}
	}

	ret = sqfs_xattr_writer_end(sqfs->xwr, &node->xattr_idx);
	if (ret) {
		sqfs_perror(hdr->name, "completing xattr block", ret);
		return -1;
	}

	return 0;
}

static int create_node_and_repack_data(FILE *input_file, sqfs_writer_t *sqfs,
				       tar_header_decoded_t *hdr)
{
	tree_node_t *node;

	if (hdr->is_hard_link) {
		node = fstree_add_hard_link(&sqfs->fs, hdr->name,
					    hdr->link_target);
		if (node == NULL)
			goto fail_errno;

		if (!cfg.quiet) {
			printf("Hard link %s -> %s\n", hdr->name,
			       hdr->link_target);
		}
		return 0;
	}

	if (!keep_time) {
		hdr->sb.st_mtime = sqfs->fs.defaults.st_mtime;
	}

	node = fstree_add_generic(&sqfs->fs, hdr->name,
				  &hdr->sb, hdr->link_target);
	if (node == NULL)
		goto fail_errno;

	if (!cfg.quiet)
		printf("Packing %s\n", hdr->name);

	if (!cfg.no_xattr) {
		if (copy_xattr(sqfs, node, hdr))
			return -1;
	}

	if (S_ISREG(hdr->sb.st_mode)) {
		if (write_file(input_file, sqfs, hdr, &node->data.file,
			       hdr->sb.st_size)) {
			return -1;
		}
	}

	return 0;
fail_errno:
	perror(hdr->name);
	return -1;
}

static int set_root_attribs(sqfs_writer_t *sqfs,
			    const tar_header_decoded_t *hdr)
{
	if (hdr->is_hard_link || !S_ISDIR(hdr->sb.st_mode)) {
		fprintf(stderr, "'%s' is not a directory!\n", hdr->name);
		return -1;
	}

	sqfs->fs.root->uid = hdr->sb.st_uid;
	sqfs->fs.root->gid = hdr->sb.st_gid;
	sqfs->fs.root->mode = hdr->sb.st_mode;

	if (keep_time)
		sqfs->fs.root->mod_time = hdr->sb.st_mtime;

	if (!cfg.no_xattr) {
		if (copy_xattr(sqfs, sqfs->fs.root, hdr))
			return -1;
	}

	return 0;
}

int process_tarball(FILE *input_file, sqfs_writer_t *sqfs)
{
	bool skip, is_root, is_prefixed;
	tar_header_decoded_t hdr;
	sqfs_u64 offset, count;
	sparse_map_t *m;
	size_t rootlen;
	int ret;

	rootlen = root_becomes == NULL ? 0 : strlen(root_becomes);

	for (;;) {
		ret = read_header(input_file, &hdr);
		if (ret > 0)
			break;
		if (ret < 0)
			return -1;

		if (hdr.mtime < 0)
			hdr.mtime = 0;

		if ((sqfs_u64)hdr.mtime > 0x0FFFFFFFFUL)
			hdr.mtime = 0x0FFFFFFFFUL;

		hdr.sb.st_mtime = hdr.mtime;

		skip = false;
		is_root = false;
		is_prefixed = true;

		if (hdr.name == NULL || canonicalize_name(hdr.name) != 0) {
			fprintf(stderr, "skipping '%s' (invalid name)\n",
				hdr.name);
			skip = true;
		}

		if (root_becomes != NULL) {
			if (strncmp(hdr.name, root_becomes, rootlen) == 0) {
				if (hdr.name[rootlen] == '\0') {
					is_root = true;
				} else if (hdr.name[rootlen] != '/') {
					is_prefixed = false;
				}
			} else {
				is_prefixed = false;
			}

			if (is_prefixed && !is_root) {
				memmove(hdr.name, hdr.name + rootlen + 1,
					strlen(hdr.name + rootlen + 1) + 1);
			}

			if (is_prefixed && hdr.name[0] == '\0') {
				fputs("skipping entry with empty name\n",
				      stderr);
				skip = true;
			}
		} else if (hdr.name[0] == '\0') {
			is_root = true;
		}

		if (!is_prefixed) {
			clear_header(&hdr);
			continue;
		}

		if (is_root) {
			if (set_root_attribs(sqfs, &hdr))
				goto fail;
			clear_header(&hdr);
			continue;
		}

		if (!skip && hdr.unknown_record) {
			fprintf(stderr, "%s: unknown entry type\n", hdr.name);
			skip = true;
		}

		if (!skip && hdr.sparse != NULL) {
			offset = hdr.sparse->offset;
			count = 0;

			for (m = hdr.sparse; m != NULL; m = m->next) {
				if (m->offset < offset) {
					skip = true;
					break;
				}
				offset = m->offset + m->count;
				count += m->count;
			}

			if (count != hdr.record_size)
				skip = true;

			if (skip) {
				fprintf(stderr, "%s: broken sparse "
					"file layout\n", hdr.name);
			}
		}

		if (skip) {
			if (dont_skip)
				goto fail;
			if (skip_entry(input_file, hdr.sb.st_size))
				goto fail;

			clear_header(&hdr);
			continue;
		}

		if (create_node_and_repack_data(input_file, sqfs, &hdr))
			goto fail;

		clear_header(&hdr);
	}

	return 0;
fail:
	clear_header(&hdr);
	return -1;
}
