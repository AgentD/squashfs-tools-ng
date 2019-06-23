/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "data_writer.h"
#include "highlevel.h"
#include "squashfs.h"
#include "compress.h"
#include "id_table.h"
#include "fstree.h"
#include "util.h"
#include "tar.h"

#include <sys/sysmacros.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>

static struct option long_opts[] = {
	{ "compressor", required_argument, NULL, 'c' },
	{ "comp-extra", required_argument, NULL, 'X' },
	{ "force", no_argument, NULL, 'f' },
	{ "quiet", no_argument, NULL, 'q' },
	{ "help", no_argument, NULL, 'h' },
	{ "version", no_argument, NULL, 'V' },
};

static const char *short_opts = "c:X:fqhV";

static const char *usagestr =
"Usage: tar2sqfs [OPTIONS...] <sqfsfile>\n"
"\n"
"Read an uncompressed tar archive from stdin and turn it into a squashfs\n"
"filesystem image.\n"
"\n"
"Possible options:\n"
"\n"
"  --compressor, -c <name>     Select the compressor to use.\n"
"                              A list of available compressors is below.\n"
"  --comp-extra, -X <options>  A comma seperated list of extra options for\n"
"                              the selected compressor. Specify 'help' to\n"
"                              get a list of available options.\n"
"  --force, -f                 Overwrite the output file if it exists.\n"
"  --quiet, -q                 Do not print out progress reports.\n"
"  --help, -h                  Print help text and exit.\n"
"  --version, -V               Print version information and exit.\n"
"\n"
"Examples:\n"
"\n"
"\ttar2sqfs rootfs.sqfs < rootfs.tar\n"
"\tzcat rootfs.tar.gz | tar2sqfs rootfs.sqfs\n"
"\txzcat rootfs.tar.xz | tar2sqfs rootfs.sqfs\n"
"\n";

static const char *filename;
static int block_size = SQFS_DEFAULT_BLOCK_SIZE;
static size_t devblksize = SQFS_DEVBLK_SIZE;
static bool quiet = false;
static int outmode = O_WRONLY | O_CREAT | O_EXCL;
static E_SQFS_COMPRESSOR comp_id;
static char *comp_extra = NULL;

static void process_args(int argc, char **argv)
{
	bool have_compressor;
	int i;

	comp_id = compressor_get_default();

	for (;;) {
		i = getopt_long(argc, argv, short_opts, long_opts, NULL);
		if (i == -1)
			break;

		switch (i) {
		case 'c':
			have_compressor = true;

			if (compressor_id_from_name(optarg, &comp_id))
				have_compressor = false;

			if (!compressor_exists(comp_id))
				have_compressor = false;

			if (!have_compressor) {
				fprintf(stderr, "Unsupported compressor '%s'\n",
					optarg);
				exit(EXIT_FAILURE);
			}
			break;
		case 'X':
			comp_extra = optarg;
			break;
		case 'f':
			outmode = O_WRONLY | O_CREAT | O_TRUNC;
			break;
		case 'q':
			quiet = true;
			break;
		case 'h':
			fputs(usagestr, stdout);
			compressor_print_available();
			exit(EXIT_SUCCESS);
		case 'V':
			print_version();
			exit(EXIT_SUCCESS);
		default:
			goto fail_arg;
		}
	}

	if (comp_extra != NULL && strcmp(comp_extra, "help") == 0) {
		compressor_print_help(comp_id);
		exit(EXIT_SUCCESS);
	}

	if (optind >= argc) {
		fputs("Missing argument: squashfs image\n", stderr);
		goto fail_arg;
	}

	filename = argv[optind++];

	if (optind < argc) {
		fputs("Unknown extra arguments\n", stderr);
		goto fail_arg;
	}
	return;
fail_arg:
	fputs("Try `tar2sqfs --help' for more information.\n", stderr);
	exit(EXIT_FAILURE);
}

static int create_node_and_repack_data(tar_header_decoded_t *hdr, fstree_t *fs,
				       data_writer_t *data)
{
	tree_node_t *node;

	node = fstree_add_generic(fs, hdr->name, &hdr->sb, hdr->link_target);
	if (node == NULL)
		goto fail_errno;

	if (!quiet)
		printf("Packing %s\n", hdr->name);

	if (S_ISREG(hdr->sb.st_mode)) {
		if (write_data_from_fd(data, node->data.file,
				       STDIN_FILENO)) {
			return -1;
		}

		if (skip_padding(STDIN_FILENO, node->data.file->size))
			return -1;
	}

	return 0;
fail_errno:
	perror(hdr->name);
	return -1;
}

static int process_tar_ball(fstree_t *fs, data_writer_t *data)
{
	tar_header_decoded_t hdr;
	int ret;

	for (;;) {
		ret = read_header(STDIN_FILENO, &hdr);
		if (ret > 0)
			break;
		if (ret < 0)
			return -1;

		if (hdr.unknown_record) {
			fprintf(stderr, "skipping '%s' (unknown entry type)\n",
				hdr.name);
			if (skip_entry(STDIN_FILENO, hdr.sb.st_size))
				goto fail;
			continue;
		}

		if (canonicalize_name(hdr.name)) {
			fprintf(stderr, "skipping '%s' (invalid name)\n",
				hdr.name);
			if (skip_entry(STDIN_FILENO, hdr.sb.st_size))
				goto fail;
			continue;
		}

		if (create_node_and_repack_data(&hdr, fs, data))
			goto fail;

		clear_header(&hdr);
	}

	return 0;
fail:
	clear_header(&hdr);
	return -1;
}

int main(int argc, char **argv)
{
	int outfd, status = EXIT_SUCCESS;
	data_writer_t *data;
	sqfs_super_t super;
	compressor_t *cmp;
	id_table_t idtbl;
	fstree_t fs;
	int ret;

	process_args(argc, argv);

	outfd = open(filename, outmode, 0644);
	if (outfd < 0) {
		perror(filename);
		return EXIT_FAILURE;
	}

	if (fstree_init(&fs, block_size, NULL))
		goto out_fd;

	cmp = compressor_create(comp_id, true, block_size, comp_extra);
	if (cmp == NULL) {
		fputs("Error creating compressor\n", stderr);
		goto out_fs;
	}

	if (sqfs_super_init(&super, block_size, fs.defaults.st_mtime, comp_id))
		goto out_cmp;

	if (sqfs_super_write(&super, outfd))
		goto out_cmp;

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

	if (id_table_init(&idtbl))
		goto out_data;

	if (process_tar_ball(&fs, data))
		goto out;

	if (data_writer_flush_fragments(data))
		goto out;

	tree_node_sort_recursive(fs.root);
	if (fstree_gen_inode_table(&fs))
		goto out;

	super.inode_count = fs.inode_tbl_size - 2;

	if (sqfs_serialize_fstree(outfd, &super, &fs, cmp, &idtbl))
		goto out;

	if (data_writer_write_fragment_table(data))
		goto out;

	if (id_table_write(&idtbl, outfd, &super, cmp))
		goto out;

	if (sqfs_super_write(&super, outfd))
		goto out;

	if (padd_file(outfd, super.bytes_used, devblksize))
		goto out;

	status = EXIT_SUCCESS;
out:
	id_table_cleanup(&idtbl);
out_data:
	data_writer_destroy(data);
out_cmp:
	cmp->destroy(cmp);
out_fs:
	fstree_cleanup(&fs);
out_fd:
	close(outfd);
	return status;
}
