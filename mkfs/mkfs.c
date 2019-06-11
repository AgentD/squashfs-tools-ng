/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "mkfs.h"
#include "util.h"

static int padd_file(int outfd, sqfs_super_t *super, options_t *opt)
{
	size_t padd_sz = super->bytes_used % opt->devblksz;
	uint8_t *buffer;
	ssize_t ret;

	if (padd_sz == 0)
		return 0;

	padd_sz = opt->devblksz - padd_sz;

	buffer = calloc(1, padd_sz);
	if (buffer == NULL) {
		perror("padding output file to block size");
		return -1;
	}

	ret = write_retry(outfd, buffer, padd_sz);

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
	data_writer_t *data;
	sqfs_super_t super;
	compressor_t *cmp;
	id_table_t idtbl;
	options_t opt;
	fstree_t fs;
	int outfd;

	process_command_line(&opt, argc, argv);

	if (sqfs_super_init(&super, opt.blksz, opt.def_mtime, opt.compressor))
		return EXIT_FAILURE;

	if (id_table_init(&idtbl))
		return EXIT_FAILURE;

	outfd = open(opt.outfile, opt.outmode, 0644);
	if (outfd < 0) {
		perror(opt.outfile);
		goto out_idtbl;
	}

	if (sqfs_super_write(&super, outfd))
		goto out_outfd;

	if (fstree_init(&fs, opt.blksz, opt.def_mtime, opt.def_mode,
			opt.def_uid, opt.def_gid)) {
		goto out_outfd;
	}

	if (opt.infile != NULL) {
		if (fstree_from_file(&fs, opt.infile, opt.packdir))
			goto out_fstree;
	} else {
		if (fstree_from_dir(&fs, opt.packdir))
			goto out_fstree;
	}

#ifdef WITH_SELINUX
	if (opt.selinux != NULL) {
		if (fstree_relabel_selinux(&fs, opt.selinux))
			goto out_fstree;
	}
#endif

	fstree_xattr_deduplicate(&fs);

	fstree_sort(&fs);

	if (fstree_gen_inode_table(&fs))
		goto out_fstree;

	super.inode_count = fs.inode_tbl_size - 2;

	cmp = compressor_create(super.compression_id, true, super.block_size,
				opt.comp_extra);
	if (cmp == NULL) {
		fputs("Error creating compressor\n", stderr);
		goto out_outfd;
	}

	ret = cmp->write_options(cmp, outfd);
	if (ret < 0)
		goto out_cmp;

	if (ret > 0) {
		super.flags |= SQFS_FLAG_COMPRESSOR_OPTIONS;
		super.bytes_used += ret;
	}

	data = data_writer_create(&super, cmp, outfd);
	if (data == NULL)
		goto out_cmp;

	if (write_data_to_image(data, &fs, &opt))
		goto out_data;

	if (sqfs_serialize_fstree(outfd, &super, &fs, cmp, &idtbl))
		goto out_data;

	if (data_writer_write_fragment_table(data))
		goto out_data;

	if (id_table_write(&idtbl, outfd, &super, cmp))
		goto out_data;

	if (write_xattr(outfd, &fs, &super, cmp))
		goto out_data;

	if (sqfs_super_write(&super, outfd))
		goto out_data;

	if (padd_file(outfd, &super, &opt))
		goto out_data;

	status = EXIT_SUCCESS;
out_data:
	data_writer_destroy(data);
out_cmp:
	cmp->destroy(cmp);
out_fstree:
	fstree_cleanup(&fs);
out_outfd:
	close(outfd);
out_idtbl:
	id_table_cleanup(&idtbl);
	return status;
}
