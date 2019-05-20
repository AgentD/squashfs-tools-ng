/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "meta_writer.h"
#include "mkfs.h"
#include "util.h"

#include <assert.h>
#include <endian.h>

typedef struct {
	tree_node_t *node;
	uint32_t block;
	uint32_t offset;
} idx_ref_t;

typedef struct {
	size_t num_nodes;
	size_t max_nodes;
	idx_ref_t idx_nodes[];
} dir_index_t;

static int get_type(mode_t mode)
{
	switch (mode & S_IFMT) {
	case S_IFSOCK: return SQFS_INODE_SOCKET;
	case S_IFIFO:  return SQFS_INODE_FIFO;
	case S_IFLNK:  return SQFS_INODE_SLINK;
	case S_IFBLK:  return SQFS_INODE_BDEV;
	case S_IFCHR:  return SQFS_INODE_CDEV;
	case S_IFDIR:  return SQFS_INODE_DIR;
	case S_IFREG:  return SQFS_INODE_FILE;
	default:
		assert(0);
	}
}

static int dir_index_grow(dir_index_t **index)
{
	size_t size = sizeof(dir_index_t) + sizeof(idx_ref_t) * 10;
	void *new;

	if (*index == NULL) {
		new = calloc(1, size);
	} else {
		if ((*index)->num_nodes < (*index)->max_nodes)
			return 0;

		size += sizeof(idx_ref_t) * (*index)->num_nodes;
		new = realloc(*index, size);
	}

	if (new == NULL) {
		perror("creating directory index");
		return -1;
	}

	*index = new;
	(*index)->max_nodes += 10;
	return 0;
}

static size_t hard_link_count(tree_node_t *n)
{
	size_t count;

	if (S_ISDIR(n->mode)) {
		count = 2;

		for (n = n->data.dir->children; n != NULL; n = n->next)
			++count;

		return count;
	}

	return 1;
}

static int write_dir(meta_writer_t *dm, dir_info_t *dir, dir_index_t **index)
{
	size_t i, size, count;
	sqfs_dir_header_t hdr;
	sqfs_dir_entry_t ent;
	tree_node_t *c, *d;
	uint32_t offset;
	uint64_t block;

	c = dir->children;
	dir->size = 0;

	meta_writer_get_position(dm, &dir->start_block, &dir->block_offset);

	while (c != NULL) {
		meta_writer_get_position(dm, &block, &offset);

		count = 0;
		size = (offset + sizeof(hdr)) % SQFS_META_BLOCK_SIZE;

		for (d = c; d != NULL; d = d->next) {
			if ((d->inode_ref >> 16) != (c->inode_ref >> 16))
				break;

			/* XXX: difference is actually signed */
			if ((d->inode_num - c->inode_num) > 0x7FFF)
				break;

			size += sizeof(ent) + strlen(c->name);

			if (count > 0 && size > SQFS_META_BLOCK_SIZE)
				break;

			count += 1;
		}

		if (count > SQFS_MAX_DIR_ENT)
			count = SQFS_MAX_DIR_ENT;

		if (dir_index_grow(index))
			return -1;

		meta_writer_get_position(dm, &block, &offset);

		i = (*index)->num_nodes++;
		(*index)->idx_nodes[i].node = c;
		(*index)->idx_nodes[i].block = block;
		(*index)->idx_nodes[i].offset = offset;

		hdr.count = htole32(count - 1);
		hdr.start_block = htole32(c->inode_ref >> 16);
		hdr.inode_number = htole32(c->inode_num);
		dir->size += sizeof(hdr);

		if (meta_writer_append(dm, &hdr, sizeof(hdr)))
			return -1;

		d = c;

		for (i = 0; i < count; ++i) {
			ent.offset = htole16(c->inode_ref & 0x0000FFFF);
			ent.inode_number = htole16(c->inode_num - d->inode_num);
			ent.type = htole16(get_type(c->mode));
			ent.size = htole16(strlen(c->name) - 1);
			dir->size += sizeof(ent) + strlen(c->name);

			if (meta_writer_append(dm, &ent, sizeof(ent)))
				return -1;
			if (meta_writer_append(dm, c->name, strlen(c->name)))
				return -1;

			c = c->next;
		}
	}
	return 0;
}

static int write_inode(sqfs_info_t *info, meta_writer_t *im, meta_writer_t *dm,
		       tree_node_t *node)
{
	dir_index_t *diridx = NULL;
	uint16_t uid_idx, gid_idx;
	file_info_t *fi = NULL;
	dir_info_t *di = NULL;
	uint32_t bs, offset;
	sqfs_inode_t base;
	uint64_t i, block;
	int type;

	if (id_table_id_to_index(&info->idtbl, node->uid, &uid_idx))
		return -1;

	if (id_table_id_to_index(&info->idtbl, node->gid, &gid_idx))
		return -1;

	meta_writer_get_position(im, &block, &offset);
	node->inode_ref = (block << 16) | offset;
	node->inode_num = info->inode_counter++;

	info->super.inode_count += 1;

	type = get_type(node->mode);

	if (node->xattr != NULL)
		type = SQFS_INODE_EXT_TYPE(type);

	if (S_ISDIR(node->mode)) {
		di = node->data.dir;

		if (write_dir(dm, di, &diridx))
			return -1;

		if ((di->start_block) > 0xFFFFFFFFUL || di->size > 0xFFFF ||
		    (node->xattr != NULL && di->size != 0)) {
			type = SQFS_INODE_EXT_DIR;
		} else {
			type = SQFS_INODE_DIR;
			free(diridx);
			diridx = NULL;
		}
	} else if (S_ISREG(node->mode)) {
		fi = node->data.file;

		if (fi->startblock > 0xFFFFFFFFUL || fi->size > 0xFFFFFFFFUL ||
		    hard_link_count(node) > 1) {
			type = SQFS_INODE_EXT_FILE;
		}
	}

	base.type = htole16(type);
	base.mode = htole16(node->mode);
	base.uid_idx = htole16(uid_idx);
	base.gid_idx = htole16(gid_idx);
	base.mod_time = htole32(info->opt.def_mtime);
	base.inode_number = htole32(node->inode_num);

	if (meta_writer_append(im, &base, sizeof(base))) {
		free(diridx);
		return -1;
	}

	switch (type) {
	case SQFS_INODE_FIFO:
	case SQFS_INODE_SOCKET: {
		sqfs_inode_ipc_t ipc = {
			.nlink = hard_link_count(node),
		};

		return meta_writer_append(im, &ipc, sizeof(ipc));
	}
	case SQFS_INODE_EXT_FIFO:
	case SQFS_INODE_EXT_SOCKET: {
		sqfs_inode_ipc_ext_t ipc = {
			.nlink = hard_link_count(node),
			.xattr_idx = htole32(0xFFFFFFFF),
		};

		if (node->xattr != NULL)
			ipc.xattr_idx = htole32(node->xattr->index);

		return meta_writer_append(im, &ipc, sizeof(ipc));
	}
	case SQFS_INODE_SLINK: {
		sqfs_inode_slink_t slink = {
			.nlink = htole32(hard_link_count(node)),
			.target_size = htole32(strlen(node->data.slink_target)),
		};

		if (meta_writer_append(im, &slink, sizeof(slink)))
			return -1;
		if (meta_writer_append(im, node->data.slink_target,
				       le32toh(slink.target_size))) {
			return -1;
		}
		break;
	}
	case SQFS_INODE_EXT_SLINK: {
		sqfs_inode_slink_t slink = {
			.nlink = htole32(hard_link_count(node)),
			.target_size = htole32(strlen(node->data.slink_target)),
		};
		uint32_t xattr = htole32(0xFFFFFFFF);

		if (node->xattr != NULL)
			xattr = htole32(node->xattr->index);

		if (meta_writer_append(im, &slink, sizeof(slink)))
			return -1;
		if (meta_writer_append(im, node->data.slink_target,
				       le32toh(slink.target_size))) {
			return -1;
		}
		if (meta_writer_append(im, &xattr, sizeof(xattr)))
			return -1;
		break;
	}
	case SQFS_INODE_BDEV:
	case SQFS_INODE_CDEV: {
		sqfs_inode_dev_t dev = {
			.nlink = htole32(hard_link_count(node)),
			.devno = htole32(node->data.devno),
		};

		return meta_writer_append(im, &dev, sizeof(dev));
	}
	case SQFS_INODE_EXT_BDEV:
	case SQFS_INODE_EXT_CDEV: {
		sqfs_inode_dev_ext_t dev = {
			.nlink = htole32(hard_link_count(node)),
			.devno = htole32(node->data.devno),
			.xattr_idx = htole32(0xFFFFFFFF),
		};

		if (node->xattr != NULL)
			dev.xattr_idx = htole32(node->xattr->index);

		return meta_writer_append(im, &dev, sizeof(dev));
	}

	case SQFS_INODE_EXT_FILE: {
		sqfs_inode_file_ext_t ext = {
			.blocks_start = htole64(fi->startblock),
			.file_size = htole64(fi->size),
			.sparse = htole64(0xFFFFFFFFFFFFFFFFUL),
			.nlink = htole32(hard_link_count(node)),
			.fragment_idx = htole32(fi->fragment),
			.fragment_offset = htole32(fi->fragment_offset),
			.xattr_idx = htole32(0xFFFFFFFF),
		};

		if (node->xattr != NULL)
			ext.xattr_idx = htole32(node->xattr->index);

		if (meta_writer_append(im, &ext, sizeof(ext)))
			return -1;
		goto out_file_blocks;
	}
	case SQFS_INODE_FILE: {
		sqfs_inode_file_t reg = {
			.blocks_start = htole32(fi->startblock),
			.fragment_index = htole32(fi->fragment),
			.fragment_offset = htole32(fi->fragment_offset),
			.file_size = htole32(fi->size),
		};

		if (meta_writer_append(im, &reg, sizeof(reg)))
			return -1;
		goto out_file_blocks;
	}
	case SQFS_INODE_DIR: {
		sqfs_inode_dir_t dir = {
			.start_block = htole32(node->data.dir->start_block),
			.nlink = htole32(hard_link_count(node)),
			.size = htole16(node->data.dir->size),
			.offset = htole16(node->data.dir->block_offset),
			.parent_inode = node->parent ?
				htole32(node->parent->inode_num) : htole32(1),
		};

		return meta_writer_append(im, &dir, sizeof(dir));
	}
	case SQFS_INODE_EXT_DIR: {
		sqfs_dir_index_t idx;
		size_t i;
		sqfs_inode_dir_ext_t ext = {
			.nlink = htole32(hard_link_count(node)),
			.size = htole32(node->data.dir->size),
			.start_block = htole32(node->data.dir->start_block),
			.parent_inode = node->parent ?
				htole32(node->parent->inode_num) : htole32(1),
			.inodex_count = htole32(diridx->num_nodes - 1),
			.offset = htole16(node->data.dir->block_offset),
			.xattr_idx = htole32(0xFFFFFFFF),
		};

		if (node->xattr != NULL)
			ext.xattr_idx = htole32(node->xattr->index);

		if (meta_writer_append(im, &ext, sizeof(ext))) {
			free(diridx);
			return -1;
		}

		for (i = 0; i < diridx->num_nodes; ++i) {
			idx.start_block = htole32(diridx->idx_nodes[i].block);

			idx.index = diridx->idx_nodes[i].offset;
			idx.index -= node->data.dir->block_offset;
			idx.index = htole32(idx.index);

			idx.size = strlen(diridx->idx_nodes[i].node->name) - 1;
			idx.size = htole32(idx.size);

			if (meta_writer_append(im, &idx, sizeof(idx))) {
				free(diridx);
				return -1;
			}

			if (meta_writer_append(im,
					       diridx->idx_nodes[i].node->name,
					       le32toh(idx.size) + 1)) {
				free(diridx);
				return -1;
			}
		}

		free(diridx);
		break;
	}
	default:
		assert(0);
	}
	return 0;
out_file_blocks:
	for (i = 0; i < fi->size / info->super.block_size; ++i) {
		bs = htole32(fi->blocksizes[i]);

		if (meta_writer_append(im, &bs, sizeof(bs)))
			return -1;
	}
	return 0;
}

static int write_child_inodes(sqfs_info_t *info, meta_writer_t *im,
			      meta_writer_t *dm, tree_node_t *root)
{
	bool has_subdirs = false;
	tree_node_t *it;

	for (it = root->data.dir->children; it != NULL; it = it->next) {
		if (S_ISDIR(it->mode)) {
			has_subdirs = true;
			break;
		}
	}

	if (has_subdirs) {
		for (it = root->data.dir->children; it != NULL; it = it->next) {
			if (!S_ISDIR(it->mode))
				continue;

			if (write_child_inodes(info, im, dm, it))
				return -1;
		}
	}

	for (it = root->data.dir->children; it != NULL; it = it->next) {
		if (write_inode(info, im, dm, it))
			return -1;
	}

	return 0;
}

int sqfs_write_inodes(sqfs_info_t *info)
{
	meta_writer_t *im, *dm;
	uint8_t buffer[1024];
	uint32_t offset;
	uint64_t block;
	size_t diff;
	ssize_t ret;
	FILE *tmp;
	int tmpfd;

	tmp = tmpfile();
	if (tmp == NULL) {
		perror("tmpfile");
		return -1;
	}

	tmpfd = fileno(tmp);

	im = meta_writer_create(info->outfd, info->cmp);
	if (im == NULL)
		goto fail_tmp;

	dm = meta_writer_create(tmpfd, info->cmp);
	if (dm == NULL)
		goto fail_im;

	info->inode_counter = 2;

	if (write_child_inodes(info, im, dm, info->fs.root))
		goto fail;

	if (write_inode(info, im, dm, info->fs.root))
		goto fail;

	if (meta_writer_flush(im))
		goto fail;

	if (meta_writer_flush(dm))
		goto fail;

	info->super.root_inode_ref = info->fs.root->inode_ref;

	meta_writer_get_position(im, &block, &offset);
	info->super.inode_table_start = info->super.bytes_used;
	info->super.bytes_used += block;

	info->super.directory_table_start = info->super.bytes_used;
	meta_writer_get_position(dm, &block, &offset);
	info->super.bytes_used += block;

	if (lseek(tmpfd, 0, SEEK_SET) == (off_t)-1) {
		perror("rewind on directory temp file");
		goto fail;
	}

	for (;;) {
		ret = read_retry(tmpfd, buffer, sizeof(buffer));

		if (ret < 0) {
			perror("read from temp file");
			goto fail;
		}
		if (ret == 0)
			break;

		diff = ret;
		ret = write_retry(info->outfd, buffer, diff);

		if (ret < 0) {
			perror("write to image file");
			goto fail;
		}
		if ((size_t)ret < diff) {
			fputs("copying meta data to image file: "
			      "truncated write\n", stderr);
			goto fail;
		}
	}

	meta_writer_destroy(dm);
	meta_writer_destroy(im);
	fclose(tmp);
	return 0;
fail:
	meta_writer_destroy(dm);
fail_im:
	meta_writer_destroy(im);
fail_tmp:
	fclose(tmp);
	return -1;
}
