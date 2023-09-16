/* SPDX-License-Identifier: 0BSD */
/*
 * extract_one.c
 *
 * Copyright (C) 2021 Luca Boccassi <luca.boccassi@microsoft.com>
 */

#include "sqfs/compressor.h"
#include "sqfs/data_reader.h"
#include "sqfs/dir_reader.h"
#include "sqfs/dir_entry.h"
#include "sqfs/error.h"
#include "sqfs/id_table.h"
#include "sqfs/io.h"
#include "sqfs/inode.h"
#include "sqfs/super.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static int find_file(sqfs_dir_iterator_t *it, const char *path,
		     sqfs_istream_t **out)
{
	const char *end = strchr(path, '/');
	size_t len = end == NULL ? strlen(path) : (size_t)(end - path);

	for (;;) {
		sqfs_dir_entry_t *ent;
		int ret;

		ret = it->next(it, &ent);
		if (ret < 0) {
			fputs("error reading directory entry\n", stderr);
			return -1;
		}

		if (ret > 0) {
			fputs("entry not found\n", stderr);
			return -1;
		}

		if (strncmp(ent->name, path, len) != 0 ||
		    ent->name[len] != '\0') {
			free(ent);
			continue;
		}

		if (end == NULL) {
			ret = it->open_file_ro(it, out);
			if (ret) {
				fprintf(stderr, "%s: error opening file %d\n",
					ent->name, ret);
			}
		} else {
			sqfs_dir_iterator_t *sub;

			ret = it->open_subdir(it, &sub);
			if (ret != 0) {
				fprintf(stderr, "%s: error opening subdir\n",
					ent->name);
			} else {
				ret = find_file(sub, end + 1, out);
				sqfs_drop(sub);
			}
		}

		free(ent);
		return ret;
	}
}

int main(int argc, char **argv)
{
	sqfs_inode_generic_t *iroot = NULL;
	sqfs_data_reader_t *data = NULL;
	sqfs_dir_reader_t *dirrd = NULL;
	sqfs_compressor_t *cmp = NULL;
	sqfs_id_table_t *idtbl = NULL;
	sqfs_file_t *file = NULL;
	sqfs_dir_iterator_t *it = NULL;
	sqfs_compressor_config_t cfg;
	sqfs_super_t super;
	int status = EXIT_FAILURE, ret;
	sqfs_istream_t *is = NULL;
	char buffer[512];

	if (argc != 3) {
		fputs("Usage: extract_one <squashfs-file> <source-file-path>\n", stderr);
		return EXIT_FAILURE;
	}

	ret = sqfs_file_open(&file, argv[1], SQFS_FILE_OPEN_READ_ONLY);
	if (ret) {
		fprintf(stderr, "%s: error opening file.\n", argv[1]);
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

	if (sqfs_dir_reader_get_root_inode(dirrd, &iroot)) {
		fprintf(stderr, "%s: error reading root inode.\n", argv[1]);
		goto out;
	}

	ret = sqfs_dir_iterator_create(dirrd, idtbl, data, NULL, iroot, &it);
	free(iroot);

	if (ret) {
		fprintf(stderr, "%s: error creating root iterator.\n",
			argv[2]);
		goto out;
	}

	if (find_file(it, argv[2], &is))
		goto out;

	for (;;) {
		ret = sqfs_istream_read(is, buffer, sizeof(buffer));
		if (ret < 0) {
			fprintf(stderr, "%s: read error!\n", argv[2]);
			goto out;
		}
		if (ret == 0)
			break;

		fwrite(buffer, 1, ret, stdout);
	}

	status = EXIT_SUCCESS;
out:
	sqfs_drop(is);
	sqfs_drop(it);
	sqfs_drop(data);
	sqfs_drop(dirrd);
	sqfs_drop(idtbl);
	sqfs_drop(cmp);
	sqfs_drop(file);
	return status;
}
