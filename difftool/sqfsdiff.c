/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * sqfsdiff.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "sqfsdiff.h"

static int open_sfqs(sqfs_state_t *state, const char *path)
{
	state->file = sqfs_open_file(path, SQFS_FILE_OPEN_READ_ONLY);
	if (state->file == NULL) {
		perror(path);
		return -1;
	}

	if (sqfs_super_read(&state->super, state->file)) {
		fprintf(stderr, "error reading super block from %s\n",
			path);
		goto fail_file;
	}

	if (!sqfs_compressor_exists(state->super.compression_id)) {
		fprintf(stderr, "%s: unknown compressor used.\n",
			path);
		goto fail_file;
	}

	sqfs_compressor_config_init(&state->cfg, state->super.compression_id,
				    state->super.block_size,
				    SQFS_COMP_FLAG_UNCOMPRESS);

	state->cmp = sqfs_compressor_create(&state->cfg);
	if (state->cmp == NULL) {
		fprintf(stderr, "%s: error creating compressor.\n", path);
		goto fail_file;
	}

	if (state->super.flags & SQFS_FLAG_COMPRESSOR_OPTIONS) {
		if (state->cmp->read_options(state->cmp, state->file)) {
			fprintf(stderr, "%s: error loading compressor "
				"options.\n", path);
			goto fail_cmp;
		}
	}

	state->idtbl = sqfs_id_table_create();
	if (state->idtbl == NULL) {
		perror("error creating ID table");
		goto fail_cmp;
	}

	if (sqfs_id_table_read(state->idtbl, state->file,
			       &state->super, state->cmp)) {
		fprintf(stderr, "%s: error loading ID table\n", path);
		goto fail_id;
	}

	state->dr = sqfs_dir_reader_create(&state->super, state->cmp,
					   state->file);
	if (state->dr == NULL) {
		perror("creating directory reader");
		goto fail_dr;
	}

	if (sqfs_dir_reader_get_full_hierarchy(state->dr, state->idtbl,
					       NULL, 0, &state->root)) {
		fprintf(stderr, "%s: error loading file system tree\n", path);
		goto fail_dr;
	}

	state->data = data_reader_create(state->file, &state->super,
					 state->cmp);
	if (state->data == NULL) {
		fprintf(stderr, "%s: error loading file system tree\n", path);
		goto fail_tree;
	}

	return 0;
fail_tree:
	sqfs_dir_tree_destroy(state->root);
fail_dr:
	sqfs_dir_reader_destroy(state->dr);
fail_id:
	sqfs_id_table_destroy(state->idtbl);
fail_cmp:
	state->cmp->destroy(state->cmp);
fail_file:
	state->file->destroy(state->file);
	return -1;
}

static void close_sfqs(sqfs_state_t *state)
{
	data_reader_destroy(state->data);
	sqfs_dir_tree_destroy(state->root);
	sqfs_dir_reader_destroy(state->dr);
	sqfs_id_table_destroy(state->idtbl);
	state->cmp->destroy(state->cmp);
	state->file->destroy(state->file);
}

int main(int argc, char **argv)
{
	int status, ret = 0;
	sqfsdiff_t sd;

	memset(&sd, 0, sizeof(sd));
	process_options(&sd, argc, argv);

	if (sd.extract_dir != NULL) {
		if (mkdir_p(sd.extract_dir))
			return 2;
	}

	if (open_sfqs(&sd.sqfs_old, sd.old_path))
		return 2;

	if (open_sfqs(&sd.sqfs_new, sd.new_path)) {
		status = 2;
		goto out_sqfs_old;
	}

	if (sd.extract_dir != NULL) {
		if (chdir(sd.extract_dir)) {
			perror(sd.extract_dir);
			ret = -1;
			goto out;
		}
	}

	ret = node_compare(&sd, sd.sqfs_old.root, sd.sqfs_new.root);
	if (ret != 0)
		goto out;

	if (sd.compare_super) {
		ret = compare_super_blocks(&sd.sqfs_old.super,
					   &sd.sqfs_new.super);
		if (ret != 0)
			goto out;
	}
out:
	if (ret < 0) {
		status = 2;
	} else if (ret > 0) {
		status = 1;
	} else {
		status = 0;
	}
	close_sfqs(&sd.sqfs_new);
out_sqfs_old:
	close_sfqs(&sd.sqfs_old);
	return status;
}
