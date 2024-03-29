/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * init.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "simple_writer.h"
#include "compress_cli.h"
#include "common.h"

#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef HAVE_SCHED_GETAFFINITY
#include <sched.h>

static size_t os_get_num_jobs(void)
{
	cpu_set_t cpu_set;
	CPU_ZERO(&cpu_set);

	if (sched_getaffinity(0, sizeof cpu_set, &cpu_set) == -1)
		return 1;
	else
		return CPU_COUNT(&cpu_set);
}
#else
static size_t os_get_num_jobs(void)
{
	return 1;
}
#endif

void sqfs_writer_cfg_init(sqfs_writer_cfg_t *cfg)
{
	memset(cfg, 0, sizeof(*cfg));

	cfg->num_jobs = os_get_num_jobs();
	cfg->block_size = SQFS_DEFAULT_BLOCK_SIZE;
	cfg->devblksize = SQFS_DEVBLK_SIZE;
	cfg->comp_id = compressor_get_default();
}

int sqfs_writer_init(sqfs_writer_t *sqfs, const sqfs_writer_cfg_t *wrcfg)
{
	sqfs_block_processor_desc_t blkdesc;
	sqfs_compressor_config_t cfg;
	fstree_defaults_t fsd;
	int ret, flags;

	sqfs->filename = wrcfg->filename;

	if (compressor_cfg_init_options(&cfg, wrcfg->comp_id,
					wrcfg->block_size,
					wrcfg->comp_extra)) {
		return -1;
	}

	ret = sqfs_file_open(&sqfs->outfile, wrcfg->filename, wrcfg->outmode);
	if (ret) {
		sqfs_perror(wrcfg->filename, "open", ret);
		return -1;
	}

	if (parse_fstree_defaults(&fsd, wrcfg->fs_defaults))
		goto fail_file;

	if (fstree_init(&sqfs->fs, &fsd))
		goto fail_file;

	ret = sqfs_compressor_create(&cfg, &sqfs->cmp);

#ifdef WITH_LZO
	if (cfg.id == SQFS_COMP_LZO) {
		if (sqfs->cmp != NULL)
			sqfs_drop(sqfs->cmp);

		ret = lzo_compressor_create(&cfg, &sqfs->cmp);
	}
#endif

	if (ret != 0) {
		sqfs_perror(wrcfg->filename, "creating compressor", ret);
		goto fail_fs;
	}

	cfg.flags |= SQFS_COMP_FLAG_UNCOMPRESS;
	ret = sqfs_compressor_create(&cfg, &sqfs->uncmp);

#ifdef WITH_LZO
	if (cfg.id == SQFS_COMP_LZO) {
		if (ret == 0 && sqfs->uncmp != NULL)
			sqfs_drop(sqfs->uncmp);

		ret = lzo_compressor_create(&cfg, &sqfs->uncmp);
	}
#endif

	if (ret != 0) {
		sqfs_perror(wrcfg->filename, "creating uncompressor", ret);
		goto fail_cmp;
	}

	ret = sqfs_super_init(&sqfs->super, wrcfg->block_size,
			      sqfs->fs.defaults.mtime, wrcfg->comp_id);
	if (ret) {
		sqfs_perror(wrcfg->filename, "initializing super block", ret);
		goto fail_uncmp;
	}

	ret = sqfs_super_write(&sqfs->super, sqfs->outfile);
	if (ret) {
		sqfs_perror(wrcfg->filename, "writing super block", ret);
		goto fail_uncmp;
	}

	ret = sqfs->cmp->write_options(sqfs->cmp, sqfs->outfile);
	if (ret < 0) {
		sqfs_perror(wrcfg->filename, "writing compressor options", ret);
		goto fail_uncmp;
	}

	if (ret > 0)
		sqfs->super.flags |= SQFS_FLAG_COMPRESSOR_OPTIONS;

	sqfs->blkwr = sqfs_block_writer_create(sqfs->outfile, 0);
	if (sqfs->blkwr == NULL) {
		perror("creating block writer");
		goto fail_uncmp;
	}

	sqfs->fragtbl = sqfs_frag_table_create(0);
	if (sqfs->fragtbl == NULL) {
		perror("creating fragment table");
		goto fail_blkwr;
	}

	memset(&blkdesc, 0, sizeof(blkdesc));
	blkdesc.size = sizeof(blkdesc);
	blkdesc.max_block_size = wrcfg->block_size;
	blkdesc.num_workers = wrcfg->num_jobs;
	blkdesc.max_backlog = wrcfg->max_backlog;
	blkdesc.cmp = sqfs->cmp;
	blkdesc.wr = sqfs->blkwr;
	blkdesc.tbl = sqfs->fragtbl;
	blkdesc.file = sqfs->outfile;
	blkdesc.uncmp = sqfs->uncmp;

	ret = sqfs_block_processor_create_ex(&blkdesc, &sqfs->data);
	if (ret != 0) {
		sqfs_perror(wrcfg->filename, "creating data block processor",
			    ret);
		goto fail_fragtbl;
	}

	sqfs->idtbl = sqfs_id_table_create(0);
	if (sqfs->idtbl == NULL) {
		sqfs_perror(wrcfg->filename, "creating ID table",
			    SQFS_ERROR_ALLOC);
		goto fail_data;
	}

	if (!wrcfg->no_xattr) {
		sqfs->xwr = sqfs_xattr_writer_create(0);

		if (sqfs->xwr == NULL) {
			sqfs_perror(wrcfg->filename, "creating xattr writer",
				    SQFS_ERROR_ALLOC);
			goto fail_id;
		}
	}

	sqfs->im = sqfs_meta_writer_create(sqfs->outfile, sqfs->cmp, 0);
	if (sqfs->im == NULL) {
		fputs("Error creating inode meta data writer.\n", stderr);
		goto fail_xwr;
	}

	sqfs->dm = sqfs_meta_writer_create(sqfs->outfile, sqfs->cmp,
					   SQFS_META_WRITER_KEEP_IN_MEMORY);
	if (sqfs->dm == NULL) {
		fputs("Error creating directory meta data writer.\n", stderr);
		goto fail_im;
	}

	flags = 0;
	if (wrcfg->exportable)
		flags |= SQFS_DIR_WRITER_CREATE_EXPORT_TABLE;

	sqfs->dirwr = sqfs_dir_writer_create(sqfs->dm, flags);
	if (sqfs->dirwr == NULL) {
		fputs("Error creating directory table writer.\n", stderr);
		goto fail_dm;
	}

	return 0;
fail_dm:
	sqfs_drop(sqfs->dm);
fail_im:
	sqfs_drop(sqfs->im);
fail_xwr:
	sqfs_drop(sqfs->xwr);
fail_id:
	sqfs_drop(sqfs->idtbl);
fail_data:
	sqfs_drop(sqfs->data);
fail_fragtbl:
	sqfs_drop(sqfs->fragtbl);
fail_blkwr:
	sqfs_drop(sqfs->blkwr);
fail_uncmp:
	sqfs_drop(sqfs->uncmp);
fail_cmp:
	sqfs_drop(sqfs->cmp);
fail_fs:
	fstree_cleanup(&sqfs->fs);
fail_file:
	sqfs_drop(sqfs->outfile);
	return -1;
}
