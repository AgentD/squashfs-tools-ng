/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * writer.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
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

static int padd_sqfs(sqfs_file_t *file, sqfs_u64 size, size_t blocksize)
{
	size_t padd_sz = size % blocksize;
	int status = -1;
	sqfs_u8 *buffer;

	if (padd_sz == 0)
		return 0;

	padd_sz = blocksize - padd_sz;

	buffer = calloc(1, padd_sz);
	if (buffer == NULL)
		goto fail_errno;

	if (file->write_at(file, file->get_size(file),
			   buffer, padd_sz)) {
		goto fail_errno;
	}

	status = 0;
out:
	free(buffer);
	return status;
fail_errno:
	perror("padding output file to block size");
	goto out;
}

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
	sqfs_compressor_config_t cfg;
	int ret, flags;

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

	ret = sqfs_super_init(&sqfs->super, wrcfg->block_size,
			      sqfs->fs.defaults.st_mtime, wrcfg->comp_id);
	if (ret) {
		sqfs_perror(wrcfg->filename, "initializing super block", ret);
		goto fail_cmp;
	}

	ret = sqfs_super_write(&sqfs->super, sqfs->outfile);
	if (ret) {
		sqfs_perror(wrcfg->filename, "writing super block", ret);
		goto fail_cmp;
	}

	ret = sqfs->cmp->write_options(sqfs->cmp, sqfs->outfile);
	if (ret < 0) {
		sqfs_perror(wrcfg->filename, "writing compressor options", ret);
		goto fail_cmp;
	}

	if (ret > 0)
		sqfs->super.flags |= SQFS_FLAG_COMPRESSOR_OPTIONS;

	sqfs->blkwr = sqfs_block_writer_create(sqfs->outfile,
					       wrcfg->devblksize, 0);
	if (sqfs->blkwr == NULL) {
		perror("creating block writer");
		goto fail_cmp;
	}

	sqfs->fragtbl = sqfs_frag_table_create(0);
	if (sqfs->fragtbl == NULL) {
		perror("creating fragment table");
		goto fail_blkwr;
	}

	sqfs->data = sqfs_block_processor_create(sqfs->super.block_size,
						 sqfs->cmp, wrcfg->num_jobs,
						 wrcfg->max_backlog,
						 sqfs->blkwr, sqfs->fragtbl);
	if (sqfs->data == NULL) {
		perror("creating data block processor");
		goto fail_fragtbl;
	}

	sqfs->idtbl = sqfs_id_table_create(0);
	if (sqfs->idtbl == NULL) {
		sqfs_perror(wrcfg->filename, "creating ID table",
			    SQFS_ERROR_ALLOC);
		goto fail_data;
	}

	if (!wrcfg->no_xattr) {
		sqfs->xwr = sqfs_xattr_writer_create();

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
fail_cmp:
	sqfs_destroy(sqfs->cmp);
fail_fs:
	fstree_cleanup(&sqfs->fs);
fail_file:
	sqfs_destroy(sqfs->outfile);
	return -1;
}

int sqfs_writer_finish(sqfs_writer_t *sqfs, const sqfs_writer_cfg_t *cfg)
{
	int ret;

	if (!cfg->quiet)
		fputs("Waiting for remaining data blocks...\n", stdout);

	ret = sqfs_block_processor_finish(sqfs->data);
	if (ret) {
		sqfs_perror(cfg->filename, "finishing data blocks", ret);
		return -1;
	}

	if (!cfg->quiet)
		fputs("Writing inodes and directories...\n", stdout);

	sqfs->super.inode_count = sqfs->fs.unique_inode_count;

	if (sqfs_serialize_fstree(cfg->filename, sqfs))
		return -1;

	if (!cfg->quiet)
		fputs("Writing fragment table...\n", stdout);

	ret = sqfs_frag_table_write(sqfs->fragtbl, sqfs->outfile,
				    &sqfs->super, sqfs->cmp);
	if (ret) {
		sqfs_perror(cfg->filename, "writing fragment table", ret);
		return -1;
	}

	if (cfg->exportable) {
		if (!cfg->quiet)
			fputs("Writing export table...\n", stdout);


		ret = sqfs_dir_writer_write_export_table(sqfs->dirwr,
						sqfs->outfile, sqfs->cmp,
						sqfs->fs.root->inode_num,
						sqfs->fs.root->inode_ref,
						&sqfs->super);
		if (ret)
			return -1;
	}

	if (!cfg->quiet)
		fputs("Writing ID table...\n", stdout);

	ret = sqfs_id_table_write(sqfs->idtbl, sqfs->outfile,
				  &sqfs->super, sqfs->cmp);
	if (ret) {
		sqfs_perror(cfg->filename, "writing ID table", ret);
		return -1;
	}

	if (!cfg->no_xattr) {
		if (!cfg->quiet)
			fputs("Writing extended attributes...\n", stdout);

		ret = sqfs_xattr_writer_flush(sqfs->xwr, sqfs->outfile,
					      &sqfs->super, sqfs->cmp);
		if (ret) {
			sqfs_perror(cfg->filename, "writing extended attributes", ret);
			return -1;
		}
	}

	sqfs->super.bytes_used = sqfs->outfile->get_size(sqfs->outfile);

	ret = sqfs_super_write(&sqfs->super, sqfs->outfile);
	if (ret) {
		sqfs_perror(cfg->filename, "updating super block", ret);
		return -1;
	}

	if (padd_sqfs(sqfs->outfile, sqfs->super.bytes_used,
		      cfg->devblksize)) {
		return -1;
	}

	if (!cfg->quiet)
		sqfs_print_statistics(&sqfs->super, sqfs->data, sqfs->blkwr);

	return 0;
}

void sqfs_writer_cleanup(sqfs_writer_t *sqfs)
{
	if (sqfs->xwr != NULL)
		sqfs_destroy(sqfs->xwr);

	sqfs_destroy(sqfs->dirwr);
	sqfs_destroy(sqfs->dm);
	sqfs_destroy(sqfs->im);
	sqfs_destroy(sqfs->idtbl);
	sqfs_destroy(sqfs->data);
	sqfs_destroy(sqfs->blkwr);
	sqfs_destroy(sqfs->fragtbl);
	sqfs_destroy(sqfs->cmp);
	fstree_cleanup(&sqfs->fs);
	sqfs_destroy(sqfs->outfile);
}
