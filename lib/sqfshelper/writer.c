/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * writer.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "highlevel.h"
#include "util.h"

#include <string.h>

void sqfs_writer_cfg_init(sqfs_writer_cfg_t *cfg)
{
	memset(cfg, 0, sizeof(*cfg));

	cfg->num_jobs = 1;
	cfg->block_size = SQFS_DEFAULT_BLOCK_SIZE;
	cfg->devblksize = SQFS_DEVBLK_SIZE;
	cfg->comp_id = compressor_get_default();
}

int sqfs_writer_init(sqfs_writer_t *sqfs, const sqfs_writer_cfg_t *wrcfg)
{
	sqfs_compressor_config_t cfg;
	int ret;

	if (compressor_cfg_init_options(&cfg, wrcfg->comp_id,
					wrcfg->block_size,
					wrcfg->comp_extra)) {
		return -1;
	}

	sqfs->outfile = sqfs_open_file(wrcfg->filename, wrcfg->outmode);
	if (sqfs->outfile == NULL) {
		perror(wrcfg->filename);
		return -1;
	}

	if (fstree_init(&sqfs->fs, wrcfg->fs_defaults))
		goto fail_file;

	sqfs->cmp = sqfs_compressor_create(&cfg);
	if (sqfs->cmp == NULL) {
		fputs("Error creating compressor\n", stderr);
		goto fail_fs;
	}

	if (sqfs_super_init(&sqfs->super, wrcfg->block_size,
			    sqfs->fs.defaults.st_mtime, wrcfg->comp_id)) {
		goto fail_cmp;
	}

	if (sqfs_super_write(&sqfs->super, sqfs->outfile))
		goto fail_cmp;

	ret = sqfs->cmp->write_options(sqfs->cmp, sqfs->outfile);
	if (ret < 0)
		goto fail_cmp;

	if (ret > 0)
		sqfs->super.flags |= SQFS_FLAG_COMPRESSOR_OPTIONS;

	sqfs->data = sqfs_data_writer_create(sqfs->super.block_size,
					     sqfs->cmp, wrcfg->num_jobs,
					     wrcfg->max_backlog,
					     wrcfg->devblksize,
					     sqfs->outfile);
	if (sqfs->data == NULL) {
		perror("creating data block processor");
		goto fail_cmp;
	}

	register_stat_hooks(sqfs->data, &sqfs->stats);

	sqfs->idtbl = sqfs_id_table_create();
	if (sqfs->idtbl == NULL)
		goto fail_data;

	if (!wrcfg->no_xattr) {
		sqfs->xwr = sqfs_xattr_writer_create();

		if (sqfs->xwr == NULL) {
			perror("creating xattr writer");
			goto fail;
		}
	}

	return 0;
fail:
	if (sqfs->xwr != NULL)
		sqfs_xattr_writer_destroy(sqfs->xwr);
	sqfs_id_table_destroy(sqfs->idtbl);
fail_data:
	sqfs_data_writer_destroy(sqfs->data);
fail_cmp:
	sqfs->cmp->destroy(sqfs->cmp);
fail_fs:
	fstree_cleanup(&sqfs->fs);
fail_file:
	sqfs->outfile->destroy(sqfs->outfile);
	return -1;
}

int sqfs_writer_finish(sqfs_writer_t *sqfs, const sqfs_writer_cfg_t *cfg)
{
	if (sqfs_data_writer_finish(sqfs->data))
		return -1;

	tree_node_sort_recursive(sqfs->fs.root);
	if (fstree_gen_inode_table(&sqfs->fs))
		return -1;

	sqfs->super.inode_count = sqfs->fs.inode_tbl_size;

	if (sqfs_serialize_fstree(sqfs->outfile, &sqfs->super,
				  &sqfs->fs, sqfs->cmp, sqfs->idtbl)) {
		return -1;
	}

	if (sqfs_data_writer_write_fragment_table(sqfs->data, &sqfs->super))
		return -1;

	if (cfg->exportable) {
		if (write_export_table(sqfs->outfile, &sqfs->fs,
				       &sqfs->super, sqfs->cmp)) {
			return -1;
		}
	}

	if (sqfs_id_table_write(sqfs->idtbl, sqfs->outfile,
				&sqfs->super, sqfs->cmp)) {
		return -1;
	}

	if (sqfs_xattr_writer_flush(sqfs->xwr, sqfs->outfile,
				    &sqfs->super, sqfs->cmp)) {
		fputs("Error writing xattr table\n", stderr);
		return -1;
	}

	sqfs->super.bytes_used = sqfs->outfile->get_size(sqfs->outfile);

	if (sqfs_super_write(&sqfs->super, sqfs->outfile))
		return 0;

	if (padd_sqfs(sqfs->outfile, sqfs->super.bytes_used,
		      cfg->devblksize)) {
		return -1;
	}

	if (!cfg->quiet)
		sqfs_print_statistics(&sqfs->super, &sqfs->stats);

	return 0;
}

void sqfs_writer_cleanup(sqfs_writer_t *sqfs)
{
	if (sqfs->xwr != NULL)
		sqfs_xattr_writer_destroy(sqfs->xwr);
	sqfs_id_table_destroy(sqfs->idtbl);
	sqfs_data_writer_destroy(sqfs->data);
	sqfs->cmp->destroy(sqfs->cmp);
	fstree_cleanup(&sqfs->fs);
	sqfs->outfile->destroy(sqfs->outfile);
}
