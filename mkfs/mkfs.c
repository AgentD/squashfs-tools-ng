/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "mkfs.h"
#include "util.h"

static int padd_file(sqfs_info_t *info)
{
	size_t padd_sz = info->super.bytes_used % info->opt.devblksz;
	uint8_t *buffer;
	ssize_t ret;

	if (padd_sz == 0)
		return 0;

	padd_sz = info->opt.devblksz - padd_sz;

	buffer = calloc(1, padd_sz);
	if (buffer == NULL) {
		perror("padding output file to block size");
		return -1;
	}

	ret = write_retry(info->outfd, buffer, padd_sz);

	if (ret < 0) {
		perror("Error padding squashfs image to page size");
		free(buffer);
		return -1;
	}

	if ((size_t)ret < padd_sz) {
		fputs("Truncated write trying to padd squashfs image\n",
		      stderr);
		return -1;
	}

	free(buffer);
	return 0;
}

int main(int argc, char **argv)
{
	int status = EXIT_FAILURE, ret;
	sqfs_info_t info;

	memset(&info, 0, sizeof(info));

	process_command_line(&info.opt, argc, argv);

	if (sqfs_super_init(&info.super, info.opt.blksz, info.opt.def_mtime,
			    info.opt.compressor)) {
		return EXIT_FAILURE;
	}

	if (id_table_init(&info.idtbl))
		return EXIT_FAILURE;

	info.outfd = open(info.opt.outfile, info.opt.outmode, 0644);
	if (info.outfd < 0) {
		perror(info.opt.outfile);
		goto out_idtbl;
	}

	if (sqfs_super_write(&info.super, info.outfd))
		goto out_outfd;

	if (fstree_init(&info.fs, info.opt.blksz, info.opt.def_mtime,
			info.opt.def_mode, info.opt.def_uid,
			info.opt.def_gid)) {
		goto out_outfd;
	}

	if (fstree_from_file(&info.fs, info.opt.infile))
		goto out_fstree;

#ifdef WITH_SELINUX
	if (info.opt.selinux != NULL) {
		if (fstree_relabel_selinux(&info.fs, info.opt.selinux))
			goto out_fstree;
	}
#endif

	fstree_xattr_deduplicate(&info.fs);

	fstree_sort(&info.fs);

	info.cmp = compressor_create(info.super.compression_id, true,
				     info.super.block_size);
	if (info.cmp == NULL) {
		fputs("Error creating compressor\n", stderr);
		goto out_outfd;
	}

	ret = info.cmp->write_options(info.cmp, info.outfd);
	if (ret < 0)
		goto out_cmp;

	if (ret > 0) {
		info.super.flags |= SQFS_FLAG_COMPRESSOR_OPTIONS;
		info.super.bytes_used += ret;
	}

	if (write_data_to_image(&info))
		goto out_cmp;

	if (sqfs_write_inodes(&info))
		goto out_cmp;

	info.super.fragment_entry_count = info.num_fragments;

	if (sqfs_write_table(info.outfd, &info.super, info.fragments,
			     sizeof(info.fragments[0]), info.num_fragments,
			     &info.super.fragment_table_start, info.cmp)) {
		goto out_cmp;
	}

	if (id_table_write(&info.idtbl, info.outfd, &info.super, info.cmp))
		goto out_cmp;

	if (sqfs_super_write(&info.super, info.outfd))
		goto out_cmp;

	if (padd_file(&info))
		goto out_cmp;

	status = EXIT_SUCCESS;
out_cmp:
	free(info.fragments);
	info.cmp->destroy(info.cmp);
out_fstree:
	fstree_cleanup(&info.fs);
out_outfd:
	close(info.outfd);
out_idtbl:
	id_table_cleanup(&info.idtbl);
	return status;
}
