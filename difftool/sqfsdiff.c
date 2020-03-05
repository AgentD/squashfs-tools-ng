/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * sqfsdiff.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "sqfsdiff.h"

static int open_sfqs(sqfs_state_t *state, const char *path)
{
	int ret;

	state->file = sqfs_open_file(path, SQFS_FILE_OPEN_READ_ONLY);
	if (state->file == NULL) {
		perror(path);
		return -1;
	}

	ret = sqfs_super_read(&state->super, state->file);
	if (ret) {
		sqfs_perror(path, "reading super block", ret);
		goto fail_file;
	}

	ret = sqfs_compressor_exists(state->super.compression_id);

#ifdef WITH_LZO
	if (state->super.compression_id == SQFS_COMP_LZO)
		ret = true;
#endif

	if (!ret) {
		fprintf(stderr, "%s: unknown compressor used.\n",
			path);
		goto fail_file;
	}

	sqfs_compressor_config_init(&state->cfg, state->super.compression_id,
				    state->super.block_size,
				    SQFS_COMP_FLAG_UNCOMPRESS);

	ret = sqfs_compressor_create(&state->cfg, &state->cmp);

#ifdef WITH_LZO
	if (state->super.compression_id == SQFS_COMP_LZO && ret != 0)
		ret = lzo_compressor_create(&state->cfg, &state->cmp);
#endif

	if (ret != 0) {
		sqfs_perror(path, "creating compressor", ret);
		goto fail_file;
	}

	if (state->super.flags & SQFS_FLAG_COMPRESSOR_OPTIONS) {
		ret = state->cmp->read_options(state->cmp, state->file);
		if (ret) {
			sqfs_perror(path, "reading compressor options", ret);
			goto fail_cmp;
		}
	}

	state->idtbl = sqfs_id_table_create(0);
	if (state->idtbl == NULL) {
		sqfs_perror(path, "creating ID table", SQFS_ERROR_ALLOC);
		goto fail_cmp;
	}

	ret = sqfs_id_table_read(state->idtbl, state->file,
				 &state->super, state->cmp);
	if (ret) {
		sqfs_perror(path, "loading ID table", ret);
		goto fail_id;
	}

	state->dr = sqfs_dir_reader_create(&state->super, state->cmp,
					   state->file);
	if (state->dr == NULL) {
		sqfs_perror(path, "creating directory reader",
			    SQFS_ERROR_ALLOC);
		goto fail_id;
	}

	ret = sqfs_dir_reader_get_full_hierarchy(state->dr, state->idtbl,
						 NULL, 0, &state->root);
	if (ret) {
		sqfs_perror(path, "loading filesystem tree", ret);
		goto fail_dr;
	}

	state->data = sqfs_data_reader_create(state->file,
					      state->super.block_size,
					      state->cmp);
	if (state->data == NULL) {
		sqfs_perror(path, "creating data reader", SQFS_ERROR_ALLOC);
		goto fail_tree;
	}

	ret = sqfs_data_reader_load_fragment_table(state->data, &state->super);
	if (ret) {
		sqfs_perror(path, "loading fragment table", ret);
		goto fail_data;
	}

	return 0;
fail_data:
	sqfs_destroy(state->data);
fail_tree:
	sqfs_dir_tree_destroy(state->root);
fail_dr:
	sqfs_destroy(state->dr);
fail_id:
	sqfs_destroy(state->idtbl);
fail_cmp:
	sqfs_destroy(state->cmp);
fail_file:
	sqfs_destroy(state->file);
	return -1;
}

static void close_sfqs(sqfs_state_t *state)
{
	sqfs_destroy(state->data);
	sqfs_dir_tree_destroy(state->root);
	sqfs_destroy(state->dr);
	sqfs_destroy(state->idtbl);
	sqfs_destroy(state->cmp);
	sqfs_destroy(state->file);
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
