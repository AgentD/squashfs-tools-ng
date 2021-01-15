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

#ifdef HAVE_SYS_SYSINFO_H
#include <sys/sysinfo.h>

static size_t os_get_num_jobs(void)
{
	int nprocs;

	nprocs = get_nprocs_conf();
	return nprocs < 1 ? 1 : nprocs;
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
	int ret, flags;

	sqfs->filename = wrcfg->filename;

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

	ret = sqfs_compressor_create(&cfg, &sqfs->cmp);

#ifdef WITH_LZO
	if (cfg.id == SQFS_COMP_LZO) {
		if (sqfs->cmp != NULL)
			sqfs_destroy(sqfs->cmp);

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
			sqfs_destroy(sqfs->uncmp);

		ret = lzo_compressor_create(&cfg, &sqfs->uncmp);
	}
#endif

	if (ret != 0) {
		sqfs_perror(wrcfg->filename, "creating uncompressor", ret);
		goto fail_cmp;
	}

	ret = sqfs_super_init(&sqfs->super, wrcfg->block_size,
			      sqfs->fs.defaults.st_mtime, wrcfg->comp_id);
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

	sqfs->blkwr = sqfs_block_writer_create(sqfs->outfile,
					       wrcfg->devblksize, 0);
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
	sqfs_destroy(sqfs->dm);
fail_im:
	sqfs_destroy(sqfs->im);
fail_xwr:
	if (sqfs->xwr != NULL)
		sqfs_destroy(sqfs->xwr);
fail_id:
	sqfs_destroy(sqfs->idtbl);
fail_data:
	sqfs_destroy(sqfs->data);
fail_fragtbl:
	sqfs_destroy(sqfs->fragtbl);
fail_blkwr:
	sqfs_destroy(sqfs->blkwr);
fail_uncmp:
	sqfs_destroy(sqfs->uncmp);
fail_cmp:
	sqfs_destroy(sqfs->cmp);
fail_fs:
	fstree_cleanup(&sqfs->fs);
fail_file:
	sqfs_destroy(sqfs->outfile);
	return -1;
}
