/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * iterator.c
 *
 * Copyright (C) 2023 David Oberhollenzer <goliath@infraroot.at>
 */
#include "sqfs2tar.h"

enum {
	STATE_INITIALIZED = 0,
	STATE_ROOT = 1,
	STATE_ENTRY = 2,
	STATE_EOF = 3,
};

typedef struct {
	sqfs_dir_iterator_t base;

	sqfs_super_t super;

	sqfs_dir_iterator_t *src;
	sqfs_inode_generic_t *root;
	sqfs_xattr_t *root_xattr;
	sqfs_u32 root_uid;
	sqfs_u32 root_gid;

	int state;
} iterator_t;

static sqfs_dir_entry_t *create_root_entry(iterator_t *it)
{
	size_t nlen = strlen(root_becomes);
	sqfs_dir_entry_t *ent = calloc(1, sizeof(*ent) + nlen + 2);

	if (ent == NULL)
		return NULL;

	memcpy(ent->name, root_becomes, nlen);
	ent->name[nlen] = '/';

	if (it->root->base.type == SQFS_INODE_EXT_DIR) {
		ent->size = it->root->data.dir_ext.size;
	} else {
		ent->size = it->root->data.dir.size;
	}

	ent->mtime = it->root->base.mod_time;
	ent->inode = it->super.root_inode_ref;
	ent->mode = it->root->base.mode;
	ent->uid = it->root_uid;
	ent->gid = it->root_gid;
	return ent;
}

static bool keep_entry(const sqfs_dir_entry_t *ent)
{
	size_t nlen;

	if (num_subdirs == 0)
		return true;

	nlen = strlen(ent->name);

	for (size_t i = 0; i < num_subdirs; ++i) {
		size_t plen = strlen(subdirs[i]);

		if (nlen <= plen) {
			if ((nlen == plen || subdirs[i][nlen] == '/') &&
			    strncmp(subdirs[i], ent->name, nlen) == 0) {
				return true;
			}
		} else if (ent->name[plen] == '/' &&
			   strncmp(subdirs[i], ent->name, plen) == 0) {
			return true;
		}
	}

	return false;
}

static void destroy(sqfs_object_t *obj)
{
	iterator_t *it = (iterator_t *)obj;

	sqfs_xattr_list_free(it->root_xattr);
	sqfs_free(it->root);
	sqfs_drop(it->src);
	free(it);
}

static int next(sqfs_dir_iterator_t *base, sqfs_dir_entry_t **out)
{
	iterator_t *it = (iterator_t *)base;
	sqfs_dir_entry_t *ent;

	*out = NULL;

	switch (it->state) {
	case STATE_INITIALIZED:
		*out = create_root_entry(it);
		if (*out == NULL) {
			it->state = SQFS_ERROR_ALLOC;
			return SQFS_ERROR_ALLOC;
		}
		it->state = STATE_ROOT;
		return 0;
	case STATE_ROOT:
		it->state = STATE_ENTRY;
		break;
	case STATE_ENTRY:
		break;
	case STATE_EOF:
		return 1;
	default:
		return it->state;
	}

	for (;;) {
		int ret = it->src->next(it->src, &ent);
		if (ret != 0) {
			it->state = ret < 0 ? ret : STATE_EOF;
			return ret;
		}

		if (keep_entry(ent)) {
			/* XXX: skip the entry, but we MUST recurse here! */
			if (num_subdirs == 1 && !keep_as_dir &&
			    strlen(ent->name) <= strlen(subdirs[0])) {
				sqfs_free(ent);
				continue;
			}
			break;
		}

		if (S_ISDIR(ent->mode))
			it->src->ignore_subdir(it->src);

		sqfs_free(ent);
	}

	if (num_subdirs == 1 && !keep_as_dir) {
		size_t plen = strlen(subdirs[0]) + 1;

		memmove(ent->name, ent->name + plen,
			strlen(ent->name + plen) + 1);
	}

	if (root_becomes != NULL) {
		size_t nlen = strlen(ent->name);
		size_t rlen = strlen(root_becomes);
		void *new = realloc(ent, sizeof(*ent) + nlen + rlen + 2);
		if (new == NULL)
			goto fail_alloc;

		ent = new;
		memmove(ent->name + rlen + 1, ent->name, nlen + 1);
		memcpy(ent->name, root_becomes, rlen);
		ent->name[rlen] = '/';
	}

	if (S_ISDIR(ent->mode)) {
		size_t nlen = strlen(ent->name);
		void *new = realloc(ent, sizeof(*ent) + nlen + 2);
		if (new == NULL)
			goto fail_alloc;

		ent = new;
		ent->name[nlen++] = '/';
		ent->name[nlen  ] = '\0';
	}

	*out = ent;
	return 0;
fail_alloc:
	sqfs_free(ent);
	it->state = SQFS_ERROR_ALLOC;
	return SQFS_ERROR_ALLOC;
}

static int read_link(sqfs_dir_iterator_t *base, char **out)
{
	iterator_t *it = (iterator_t *)base;

	if (it->state != STATE_ENTRY)
		return it->state < 0 ? it->state : SQFS_ERROR_NO_ENTRY;

	return it->src->read_link(it->src, out);
}

static int open_file_ro(sqfs_dir_iterator_t *base, sqfs_istream_t **out)
{
	iterator_t *it = (iterator_t *)base;

	if (it->state != STATE_ENTRY)
		return it->state < 0 ? it->state : SQFS_ERROR_NO_ENTRY;

	return it->src->open_file_ro(it->src, out);
}

static int read_xattr(sqfs_dir_iterator_t *base, sqfs_xattr_t **out)
{
	iterator_t *it = (iterator_t *)base;

	if (it->state == STATE_ROOT) {
		if (it->root_xattr != NULL) {
			*out = sqfs_xattr_list_copy(it->root_xattr);
			if (*out == NULL)
				return SQFS_ERROR_ALLOC;
		} else {
			*out = NULL;
		}
		return 0;
	}

	if (it->state != STATE_ENTRY)
		return it->state < 0 ? it->state : SQFS_ERROR_NO_ENTRY;

	return it->src->read_xattr(it->src, out);
}

sqfs_dir_iterator_t *tar_compat_iterator_create(const char *filename)
{
	sqfs_dir_iterator_t *base = NULL;
	sqfs_id_table_t *idtbl = NULL;
	sqfs_compressor_t *cmp = NULL;
	sqfs_dir_reader_t *dr = NULL;
	sqfs_data_reader_t *data = NULL;
	sqfs_xattr_reader_t *xr = NULL;
	sqfs_compressor_config_t cfg;
	sqfs_file_t *file = NULL;
	iterator_t *it = NULL;
	int ret;

	it = calloc(1, sizeof(*it));
	if (it == NULL) {
		perror("tar iterator");
		return NULL;
	}

	/* open the file and read the super block */
	ret = sqfs_file_open(&file, filename, SQFS_FILE_OPEN_READ_ONLY);
	if (ret) {
		sqfs_perror(filename, NULL, ret);
		goto fail;
	}

	ret = sqfs_super_read(&it->super, file);
	if (ret) {
		sqfs_perror(filename, "reading super block", ret);
		goto fail;
	}

	/* create a compressor */
	sqfs_compressor_config_init(&cfg, it->super.compression_id,
				    it->super.block_size,
				    SQFS_COMP_FLAG_UNCOMPRESS);

	ret = sqfs_compressor_create(&cfg, &cmp);

#ifdef WITH_LZO
	if (it->super.compression_id == SQFS_COMP_LZO && ret != 0)
		ret = lzo_compressor_create(&cfg, &cmp);
#endif

	if (ret != 0) {
		sqfs_perror(filename, "creating compressor", ret);
		goto fail;
	}

	/* create a directory reader */
	dr = sqfs_dir_reader_create(&it->super, cmp, file, 0);
	if (dr == NULL) {
		sqfs_perror(filename, "creating dir reader",
			    SQFS_ERROR_ALLOC);
		goto fail;
	}

	/* load ID table */
	idtbl = sqfs_id_table_create(0);
	if (idtbl == NULL) {
		ret = SQFS_ERROR_ALLOC;
	} else {
		ret = sqfs_id_table_read(idtbl, file, &it->super, cmp);
	}

	if (ret) {
		sqfs_perror(filename, "loading ID table", ret);
		goto fail;
	}

	/* create data reader */
	data = sqfs_data_reader_create(file, it->super.block_size, cmp, 0);
	if (data == NULL) {
		sqfs_perror(filename, "creating data reader",
			    SQFS_ERROR_ALLOC);
		goto fail;
	}

	ret = sqfs_data_reader_load_fragment_table(data, &it->super);
	if (ret) {
		sqfs_perror(filename, "loading fragment table", ret);
		goto fail;
	}

	/* create xattr reader */
	if (!no_xattr && !(it->super.flags & SQFS_FLAG_NO_XATTRS)) {
		xr = sqfs_xattr_reader_create(0);
		if (xr == NULL) {
			sqfs_perror(filename, "creating xattr reader",
				    SQFS_ERROR_ALLOC);
			goto fail;
		}

		ret = sqfs_xattr_reader_load(xr, &it->super, file, cmp);
		if (ret) {
			sqfs_perror(filename, "loading xattr table", ret);
			goto fail;
		}
	}

	/* get the root inode */
	ret = sqfs_dir_reader_get_root_inode(dr, &it->root);
	if (ret) {
		sqfs_perror(filename, "reading root inode", ret);
		goto fail;
	}

	/* create a recursive directory iterator from there */
	ret = sqfs_dir_iterator_create(dr, idtbl, data, xr, it->root, &base);
	if (ret) {
		sqfs_perror(filename, "opening root directory", ret);
		goto fail;
	}

	ret = sqfs_dir_iterator_create_recursive(&it->src, base);
	base = sqfs_drop(base);
	if (ret) {
		sqfs_perror(filename, "creating directory scanner", ret);
		goto fail;
	}

	/* get root attributes if we need them */
	if (root_becomes == NULL) {
		sqfs_free(it->root);
		it->root = NULL;

		it->state = STATE_ENTRY;
	} else {
		sqfs_u32 idx;

		ret = sqfs_id_table_index_to_id(idtbl, it->root->base.uid_idx,
						&it->root_uid);
		if (ret == 0) {
			ret = sqfs_id_table_index_to_id(idtbl,
							it->root->base.gid_idx,
							&it->root_gid);
		}

		if (ret == 0)
			ret = sqfs_inode_get_xattr_index(it->root, &idx);

		if (xr != NULL && ret == 0)
			ret = sqfs_xattr_reader_read_all(xr, idx,
							 &it->root_xattr);
		if (ret) {
			sqfs_perror(filename, "reading root inode attributes",
				    ret);
			goto fail;
		}

		it->state = STATE_INITIALIZED;
	}

	/* finish up initialization */
	sqfs_object_init(it, destroy, NULL);
	((sqfs_dir_iterator_t *)it)->next = next;
	((sqfs_dir_iterator_t *)it)->read_link = read_link;
	((sqfs_dir_iterator_t *)it)->open_file_ro = open_file_ro;
	((sqfs_dir_iterator_t *)it)->read_xattr = read_xattr;
out:
	xr = sqfs_drop(xr);
	dr = sqfs_drop(dr);
	cmp = sqfs_drop(cmp);
	file = sqfs_drop(file);
	idtbl = sqfs_drop(idtbl);
	data = sqfs_drop(data);
	return (sqfs_dir_iterator_t *)it;
fail:
	sqfs_free(it->root);
	sqfs_drop(it->src);
	free(it);
	it = NULL;
	goto out;
}
