/* SPDX-License-Identifier: 0BSD */
/*
 * extract_one.c
 *
 * Copyright (C) 2021 Luca Boccassi <luca.boccassi@microsoft.com>
 */

#include "sqfs/compressor.h"
#include "sqfs/data_reader.h"
#include "sqfs/dir_reader.h"
#include "sqfs/error.h"
#include "sqfs/id_table.h"
#include "sqfs/io.h"
#include "sqfs/inode.h"
#include "sqfs/super.h"
#include "sqfs/xattr_reader.h"

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char **argv)
{
	sqfs_xattr_reader_t *xattr = NULL;
	sqfs_data_reader_t *data = NULL;
	sqfs_dir_reader_t *dirrd = NULL;
	sqfs_compressor_t *cmp = NULL;
	sqfs_id_table_t *idtbl = NULL;
	sqfs_tree_node_t *n = NULL;
	sqfs_file_t *file = NULL;
	sqfs_u8 *p, *output = NULL;
	sqfs_compressor_config_t cfg;
	sqfs_super_t super;
	sqfs_u64 file_size;
	int status = EXIT_FAILURE, ret;

	if (argc != 3) {
		fputs("Usage: extract_one <squashfs-file> <source-file-path>\n", stderr);
		return EXIT_FAILURE;
	}

	file = sqfs_open_file(argv[1], SQFS_FILE_OPEN_READ_ONLY);
	if (file == NULL) {
		perror(argv[1]);
		return EXIT_FAILURE;
	}

	ret = sqfs_super_read(&super, file);
	if (ret) {
		fprintf(stderr, "%s: error reading super block.\n", argv[1]);
		goto out;
	}

	sqfs_compressor_config_init(&cfg, super.compression_id,
								super.block_size,
								SQFS_COMP_FLAG_UNCOMPRESS);

	ret = sqfs_compressor_create(&cfg, &cmp);
	if (ret != 0) {
		fprintf(stderr, "%s: error creating compressor: %d.\n", argv[1], ret);
		goto out;
	}

	if (!(super.flags & SQFS_FLAG_NO_XATTRS)) {
		xattr = sqfs_xattr_reader_create(0);
		if (xattr == NULL) {
			fprintf(stderr, "%s: error creating xattr reader: %d.\n", argv[1], SQFS_ERROR_ALLOC);
			goto out;
		}

		ret = sqfs_xattr_reader_load(xattr, &super, file, cmp);
		if (ret) {
			fprintf(stderr, "%s: error loading xattr reader: %d.\n", argv[1], ret);
			goto out;
		}
	}

	idtbl = sqfs_id_table_create(0);
	if (idtbl == NULL) {
		fprintf(stderr, "%s: error creating ID table: %d.\n", argv[1], ret);
		goto out;
	}

	ret = sqfs_id_table_read(idtbl, file, &super, cmp);
	if (ret) {
		fprintf(stderr, "%s: error loading ID table: %d.\n", argv[1], ret);
		goto out;
	}

	dirrd = sqfs_dir_reader_create(&super, cmp, file, 0);
	if (dirrd == NULL) {
		fprintf(stderr, "%s: error creating dir reader: %d.\n", argv[1], SQFS_ERROR_ALLOC);
		goto out;
	}

	data = sqfs_data_reader_create(file, super.block_size, cmp, 0);
	if (data == NULL) {
		fprintf(stderr, "%s: error creating data reader: %d.\n", argv[1], SQFS_ERROR_ALLOC);
		goto out;
	}

	ret = sqfs_data_reader_load_fragment_table(data, &super);
	if (ret) {
		fprintf(stderr, "%s: error loading fragment table: %d.\n", argv[1], ret);
		goto out;
	}

	ret = sqfs_dir_reader_get_full_hierarchy(dirrd, idtbl, argv[2], 0, &n);
	if (ret) {
		fprintf(stderr, "%s: error reading filesystem hierarchy: %d.\n", argv[1], ret);
		goto out;
	}

	if (!S_ISREG(n->inode->base.mode)) {
		fprintf(stderr, "/%s is not a file\n", argv[2]);
		goto out;
	}

	sqfs_inode_get_file_size(n->inode, &file_size);

	output = p = malloc(file_size);
	if (output == NULL) {
		fprintf(stderr, "malloc failed: %d\n", errno);
		goto out;
	}

	for (size_t i = 0; i < sqfs_inode_get_file_block_count(n->inode); ++i) {
		size_t chunk_size, read = (file_size < super.block_size) ? file_size : super.block_size;
		sqfs_u8 *chunk;

		ret = sqfs_data_reader_get_block(data, n->inode, i, &chunk_size, &chunk);
		if (ret) {
			fprintf(stderr, "reading data block: %d\n", ret);
			goto out;
		}

		memcpy(p, chunk, chunk_size);
		p += chunk_size;
		free(chunk);

		file_size -= read;
	}

	if (file_size > 0) {
		size_t chunk_size;
		sqfs_u8 *chunk;

		ret = sqfs_data_reader_get_fragment(data, n->inode, &chunk_size, &chunk);
		if (ret) {
			fprintf(stderr, "reading fragment block: %d\n", ret);
			goto out;
		}

		memcpy(p, chunk, chunk_size);
		free(chunk);
	}

	/* for example simplicity, assume this is a text file */
	fprintf(stdout, "%s\n", (char *)output);

	status = EXIT_SUCCESS;
out:
	sqfs_dir_tree_destroy(n);
	sqfs_drop(data);
	sqfs_drop(dirrd);
	sqfs_drop(idtbl);
	sqfs_drop(xattr);
	sqfs_drop(cmp);
	sqfs_drop(file);
	free(output);

	return status;
}
