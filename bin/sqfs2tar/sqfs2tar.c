/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * sqfs2tar.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "sqfs2tar.h"

sqfs_xattr_reader_t *xr;
sqfs_data_reader_t *data;
sqfs_super_t super;
ostream_t *out_file = NULL;

static sqfs_file_t *file;

char *assemble_tar_path(char *name, bool is_dir)
{
	size_t len, new_len;
	char *temp;
	(void)is_dir;

	if (root_becomes == NULL && !is_dir)
		return name;

	new_len = strlen(name);
	if (root_becomes != NULL)
		new_len += strlen(root_becomes) + 1;
	if (is_dir)
		new_len += 1;

	temp = realloc(name, new_len + 1);
	if (temp == NULL) {
		perror("assembling tar entry filename");
		free(name);
		return NULL;
	}

	name = temp;

	if (root_becomes != NULL) {
		len = strlen(root_becomes);

		memmove(name + len + 1, name, strlen(name) + 1);
		memcpy(name, root_becomes, len);
		name[len] = '/';
	}

	if (is_dir) {
		len = strlen(name);

		if (len == 0 || name[len - 1] != '/') {
			name[len++] = '/';
			name[len] = '\0';
		}
	}

	return name;
}

static int terminate_archive(void)
{
	char buffer[1024];

	memset(buffer, '\0', sizeof(buffer));

	return ostream_append(out_file, buffer, sizeof(buffer));
}

static sqfs_tree_node_t *tree_merge(sqfs_tree_node_t *lhs,
				    sqfs_tree_node_t *rhs)
{
	sqfs_tree_node_t *head = NULL, **next_ptr = &head;
	sqfs_tree_node_t *it, *l, *r;
	int diff;

	while (lhs->children != NULL && rhs->children != NULL) {
		diff = strcmp((const char *)lhs->children->name,
			      (const char *)rhs->children->name);

		if (diff < 0) {
			it = lhs->children;
			lhs->children = lhs->children->next;
		} else if (diff > 0) {
			it = rhs->children;
			rhs->children = rhs->children->next;
		} else {
			l = lhs->children;
			lhs->children = lhs->children->next;

			r = rhs->children;
			rhs->children = rhs->children->next;

			it = tree_merge(l, r);
		}

		*next_ptr = it;
		next_ptr = &it->next;
	}

	it = (lhs->children != NULL ? lhs->children : rhs->children);
	*next_ptr = it;

	sqfs_dir_tree_destroy(rhs);
	lhs->children = head;
	return lhs;
}

int main(int argc, char **argv)
{
	sqfs_tree_node_t *root = NULL, *subtree;
	int flags, ret, status = EXIT_FAILURE;
	sqfs_compressor_config_t cfg;
	sqfs_compressor_t *cmp;
	sqfs_id_table_t *idtbl;
	sqfs_dir_reader_t *dr;
	size_t i;

	process_args(argc, argv);

	out_file = ostream_open_stdout();
	if (out_file == NULL) {
		perror("changing stdout to binary mode");
		goto out_dirs;
	}

	if (compressor > 0) {
		out_file = ostream_compressor_create(out_file, compressor);
		if (out_file == NULL)
			goto out_dirs;
	}

	file = sqfs_open_file(filename, SQFS_FILE_OPEN_READ_ONLY);
	if (file == NULL) {
		perror(filename);
		goto out_ostrm;
	}

	ret = sqfs_super_read(&super, file);
	if (ret) {
		sqfs_perror(filename, "reading super block", ret);
		goto out_fd;
	}

	sqfs_compressor_config_init(&cfg, super.compression_id,
				    super.block_size,
				    SQFS_COMP_FLAG_UNCOMPRESS);

	ret = sqfs_compressor_create(&cfg, &cmp);

#ifdef WITH_LZO
	if (super.compression_id == SQFS_COMP_LZO && ret != 0)
		ret = lzo_compressor_create(&cfg, &cmp);
#endif

	if (ret != 0) {
		sqfs_perror(filename, "creating compressor", ret);
		goto out_fd;
	}

	idtbl = sqfs_id_table_create(0);

	if (idtbl == NULL) {
		perror("creating ID table");
		goto out_cmp;
	}

	ret = sqfs_id_table_read(idtbl, file, &super, cmp);
	if (ret) {
		sqfs_perror(filename, "loading ID table", ret);
		goto out_id;
	}

	data = sqfs_data_reader_create(file, super.block_size, cmp, 0);
	if (data == NULL) {
		sqfs_perror(filename, "creating data reader",
			    SQFS_ERROR_ALLOC);
		goto out_id;
	}

	ret = sqfs_data_reader_load_fragment_table(data, &super);
	if (ret) {
		sqfs_perror(filename, "loading fragment table", ret);
		goto out_data;
	}

	dr = sqfs_dir_reader_create(&super, cmp, file, 0);
	if (dr == NULL) {
		sqfs_perror(filename, "creating dir reader",
			    SQFS_ERROR_ALLOC);
		goto out_data;
	}

	if (!no_xattr && !(super.flags & SQFS_FLAG_NO_XATTRS)) {
		xr = sqfs_xattr_reader_create(0);
		if (xr == NULL) {
			sqfs_perror(filename, "creating xattr reader",
				    SQFS_ERROR_ALLOC);
			goto out_dr;
		}

		ret = sqfs_xattr_reader_load(xr, &super, file, cmp);
		if (ret) {
			sqfs_perror(filename, "loading xattr table", ret);
			goto out_xr;
		}
	}

	if (num_subdirs == 0) {
		ret = sqfs_dir_reader_get_full_hierarchy(dr, idtbl, NULL,
							 0, &root);
		if (ret) {
			sqfs_perror(filename, "loading filesystem tree", ret);
			goto out;
		}
	} else {
		flags = 0;

		if (keep_as_dir || num_subdirs > 1)
			flags = SQFS_TREE_STORE_PARENTS;

		for (i = 0; i < num_subdirs; ++i) {
			ret = sqfs_dir_reader_get_full_hierarchy(dr, idtbl,
								 subdirs[i],
								 flags,
								 &subtree);
			if (ret) {
				sqfs_perror(subdirs[i], "loading filesystem "
					    "tree", ret);
				goto out;
			}

			if (root == NULL) {
				root = subtree;
			} else {
				root = tree_merge(root, subtree);
			}
		}
	}

	if (write_tree(root))
		goto out;

	if (terminate_archive())
		goto out;

	if (ostream_flush(out_file))
		goto out;

	status = EXIT_SUCCESS;
out:
	if (root != NULL)
		sqfs_dir_tree_destroy(root);
out_xr:
	if (xr != NULL)
		sqfs_destroy(xr);
out_dr:
	sqfs_destroy(dr);
out_data:
	sqfs_destroy(data);
out_id:
	sqfs_destroy(idtbl);
out_cmp:
	sqfs_destroy(cmp);
out_fd:
	sqfs_destroy(file);
out_ostrm:
	sqfs_destroy(out_file);
out_dirs:
	for (i = 0; i < num_subdirs; ++i)
		free(subdirs[i]);
	free(subdirs);
	free(root_becomes);
	return status;
}
