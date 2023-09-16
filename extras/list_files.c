/* SPDX-License-Identifier: 0BSD */
/*
 * list_files.c
 *
 * Copyright (C) 2020 David Oberhollenzer <goliath@infraroot.at>
 */
#include "sqfs/compressor.h"
#include "sqfs/dir_reader.h"
#include "sqfs/dir_entry.h"
#include "sqfs/id_table.h"
#include "sqfs/inode.h"
#include "sqfs/super.h"
#include "sqfs/io.h"

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>

static int count_entries(sqfs_dir_iterator_t *it, unsigned int *out)
{
	sqfs_dir_entry_t *ent;
	int ret;

	*out = 0;

	for (;;) {
		ret = it->next(it, &ent);
		if (ret < 0) {
			*out = 0;
			return -1;
		}
		if (ret > 0)
			break;

		*out += 1;
		free(ent);
	}

	return 0;
}

static void write_tree_dfs(sqfs_dir_iterator_t *it,
			   unsigned int mask, unsigned int level,
			   unsigned int count)
{
	sqfs_dir_entry_t *ent;
	unsigned int i;
	sqfs_u16 type;

	while (count--) {
		for (i = 0; i < level; ++i)
			fputs(mask & (1 << i) ? "│  " : "   ", stdout);

		if (it->next(it, &ent) != 0)
			break;

		fputs(count == 0 ? "└─ " : "├─ ", stdout);
		fputs(ent->name, stdout);

		type = ent->mode & SQFS_INODE_MODE_MASK;
		free(ent);

		if (type == SQFS_INODE_MODE_LNK) {
			char *target;

			if (it->read_link(it, &target) == 0)
				printf(" ⭢ %s", target);

			free(target);
		}

		fputc('\n', stdout);

		if (type == SQFS_INODE_MODE_DIR) {
			sqfs_dir_iterator_t *sub;
			unsigned int sub_count;

			it->open_subdir(it, &sub);
			count_entries(sub, &sub_count);
			sub = sqfs_drop(sub);

			it->open_subdir(it, &sub);
			write_tree_dfs(sub,
				       mask | (count > 0 ? (1 << level) : 0),
				       level + 1, sub_count);
			sqfs_drop(sub);
		}
	}
}

static sqfs_dir_iterator_t *create_root_iterator(sqfs_dir_reader_t *dr,
						 sqfs_id_table_t *idtbl,
						 const char *filename)
{
	sqfs_inode_generic_t *iroot;
	sqfs_dir_iterator_t *it;
	int ret;

	if (sqfs_dir_reader_get_root_inode(dr, &iroot)) {
		fprintf(stderr, "%s: error reading root inode.\n",
			filename);
		return NULL;
	}

	ret = sqfs_dir_iterator_create(dr, idtbl, NULL, NULL, iroot, &it);
	free(iroot);

	if (ret) {
		fprintf(stderr, "%s: error creating root iterator.\n",
			filename);
		return NULL;
	}

	return it;
}

int main(int argc, char **argv)
{
	int ret, status = EXIT_FAILURE;
	sqfs_dir_iterator_t *it = NULL;
	sqfs_compressor_t *cmp = NULL;
	sqfs_id_table_t *idtbl = NULL;
	sqfs_dir_reader_t *dr = NULL;
	sqfs_compressor_config_t cfg;
	sqfs_file_t *file = NULL;
	unsigned int root_count;
	sqfs_super_t super;

	/* open the SquashFS file we want to read */
	if (argc != 2) {
		fputs("Usage: list_files <squashfs-file>\n", stderr);
		return EXIT_FAILURE;
	}

	if (sqfs_file_open(&file, argv[1], SQFS_FILE_OPEN_READ_ONLY)) {
		fprintf(stderr, "%s: error opening file.\n", argv[1]);
		return EXIT_FAILURE;
	}

	/* read the super block, create a compressor and
	   process the compressor options */
	if (sqfs_super_read(&super, file)) {
		fprintf(stderr, "%s: error reading super block.\n", argv[1]);
		goto out;
	}

	sqfs_compressor_config_init(&cfg, super.compression_id,
				    super.block_size,
				    SQFS_COMP_FLAG_UNCOMPRESS);

	ret = sqfs_compressor_create(&cfg, &cmp);
	if (ret != 0) {
		fprintf(stderr, "%s: error creating compressor: %d.\n",
			argv[1], ret);
		goto out;
	}

	/* Create and read the UID/GID mapping table */
	idtbl = sqfs_id_table_create(0);
	if (idtbl == NULL) {
		fputs("Error creating ID table.\n", stderr);
		goto out;
	}

	if (sqfs_id_table_read(idtbl, file, &super, cmp)) {
		fprintf(stderr, "%s: error loading ID table.\n", argv[1]);
		goto out;
	}

	/* create a directory reader */
	dr = sqfs_dir_reader_create(&super, cmp, file, 0);
	if (dr == NULL) {
		fprintf(stderr, "%s: error creating directory reader.\n",
			argv[1]);
		goto out;
	}

	/* create a directory iterator for the root inode */
	it = create_root_iterator(dr, idtbl, argv[1]);
	if (it == NULL)
		goto out;

	if (count_entries(it, &root_count)) {
		fprintf(stderr, "%s: error counting root iterator entries.\n",
			argv[1]);
		goto out;
	}

	it = sqfs_drop(it);

	it = create_root_iterator(dr, idtbl, argv[1]);
	if (it == NULL)
		goto out;

	/* fancy print the hierarchy */
	printf("/\n");
	write_tree_dfs(it, 0, 0, root_count);

	/* cleanup */
	status = EXIT_SUCCESS;
out:
	sqfs_drop(it);
	sqfs_drop(dr);
	sqfs_drop(idtbl);
	sqfs_drop(cmp);
	sqfs_drop(file);
	return status;
}
