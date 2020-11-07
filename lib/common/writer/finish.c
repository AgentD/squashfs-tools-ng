/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * finish.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "simple_writer.h"
#include "common.h"

#include <stdlib.h>

static void print_statistics(const sqfs_super_t *super,
			     const sqfs_block_processor_t *blk,
			     const sqfs_block_writer_t *wr)
{
	const sqfs_block_processor_stats_t *proc_stats;
	sqfs_u64 bytes_written, blocks_written;
	char read_sz[32], written_sz[32];
	size_t ratio;

	proc_stats = sqfs_block_processor_get_stats(blk);
	blocks_written = wr->get_block_count(wr);

	bytes_written = super->inode_table_start - sizeof(*super);

	if (proc_stats->input_bytes_read > 0) {
		ratio = (100 * bytes_written) / proc_stats->input_bytes_read;
	} else {
		ratio = 100;
	}

	print_size(proc_stats->input_bytes_read, read_sz, false);
	print_size(bytes_written, written_sz, false);

	fputs("---------------------------------------------------\n", stdout);
	printf("Data bytes read: %s\n", read_sz);
	printf("Data bytes written: %s\n", written_sz);
	printf("Data compression ratio: " PRI_SZ "%%\n", ratio);
	fputc('\n', stdout);

	printf("Data blocks written: " PRI_U64 "\n", blocks_written);
	printf("Out of which where fragment blocks: " PRI_U64 "\n",
	       proc_stats->frag_block_count);

	printf("Duplicate blocks omitted: " PRI_U64 "\n",
	       proc_stats->data_block_count + proc_stats->frag_block_count -
	       blocks_written);

	printf("Sparse blocks omitted: " PRI_U64 "\n",
	       proc_stats->sparse_block_count);
	fputc('\n', stdout);

	printf("Fragments actually written: " PRI_U64 "\n",
	       proc_stats->actual_frag_count);
	printf("Duplicated fragments omitted: " PRI_U64 "\n",
	       proc_stats->total_frag_count - proc_stats->actual_frag_count);
	printf("Total number of inodes: %u\n", super->inode_count);
	printf("Number of unique group/user IDs: %u\n", super->id_count);
	fputc('\n', stdout);
}

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
			sqfs_perror(cfg->filename,
				    "writing extended attributes", ret);
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
		print_statistics(&sqfs->super, sqfs->data, sqfs->blkwr);

	return 0;
}
