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

static size_t os_get_max_ram(void)
{
	struct sysinfo info;

	if (sysinfo(&info)) {
		perror("sysinfo");
		return 0;
	}

	return info.totalram;
}
#else
static size_t os_get_num_jobs(void)
{
	return 1;
}

static size_t os_get_max_ram(void)
{
	(void)cfg;
	return 0;
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
	size_t max_ram;

	memset(cfg, 0, sizeof(*cfg));

	cfg->num_jobs = os_get_num_jobs();
	cfg->block_size = SQFS_DEFAULT_BLOCK_SIZE;
	cfg->devblksize = SQFS_DEVBLK_SIZE;
	cfg->comp_id = compressor_get_default();

	max_ram = os_get_max_ram();
	cfg->max_backlog = (max_ram / 2) / cfg->block_size;

	if (cfg->max_backlog < 1)
		cfg->max_backlog = 10 * cfg->num_jobs;
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

	sqfs->data = sqfs_data_writer_create(sqfs->super.block_size,
					     sqfs->cmp, wrcfg->num_jobs,
					     wrcfg->max_backlog,
					     wrcfg->devblksize,
					     sqfs->outfile);
	if (sqfs->data == NULL) {
		perror("creating data block processor");
		goto fail_cmp;
	}

	memset(&sqfs->stats, 0, sizeof(sqfs->stats));
	register_stat_hooks(sqfs->data, &sqfs->stats);

	sqfs->idtbl = sqfs_id_table_create();
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
	int ret;

	if (!cfg->quiet)
		fputs("Waiting for remaining data blocks...\n", stdout);

	ret = sqfs_data_writer_finish(sqfs->data);
	if (ret) {
		sqfs_perror(cfg->filename, "finishing data blocks", ret);
		return -1;
	}

	if (!cfg->quiet)
		fputs("Writing inodes and directories...\n", stdout);

	tree_node_sort_recursive(sqfs->fs.root);
	if (fstree_gen_inode_table(&sqfs->fs))
		return -1;

	sqfs->super.inode_count = sqfs->fs.inode_tbl_size;

	if (sqfs_serialize_fstree(cfg->filename, sqfs->outfile, &sqfs->super,
				  &sqfs->fs, sqfs->cmp, sqfs->idtbl)) {
		return -1;
	}

	if (!cfg->quiet)
		fputs("Writing fragment table...\n", stdout);

	ret = sqfs_data_writer_write_fragment_table(sqfs->data, &sqfs->super);
	if (ret) {
		sqfs_perror(cfg->filename, "writing fragment table", ret);
		return -1;
	}

	if (cfg->exportable) {
		if (!cfg->quiet)
			fputs("Writing export table...\n", stdout);

		if (write_export_table(cfg->filename, sqfs->outfile, &sqfs->fs,
				       &sqfs->super, sqfs->cmp)) {
			return -1;
		}
	}

	if (!cfg->quiet)
		fputs("Writing ID table...\n", stdout);

	ret = sqfs_id_table_write(sqfs->idtbl, sqfs->outfile,
				  &sqfs->super, sqfs->cmp);
	if (ret) {
		sqfs_perror(cfg->filename, "writing ID table", ret);
		return -1;
	}

	if (!cfg->quiet)
		fputs("Writing extended attributes...\n", stdout);

	ret = sqfs_xattr_writer_flush(sqfs->xwr, sqfs->outfile,
				      &sqfs->super, sqfs->cmp);
	if (ret) {
		sqfs_perror(cfg->filename, "writing extended attributes", ret);
		return -1;
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
