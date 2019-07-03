/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "meta_reader.h"
#include "data_reader.h"
#include "highlevel.h"
#include "compress.h"
#include "fstree.h"
#include "util.h"
#include "tar.h"

#include <getopt.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>

static struct option long_opts[] = {
	{ "no-skip", no_argument, NULL, 's' },
	{ "help", no_argument, NULL, 'h' },
	{ "version", no_argument, NULL, 'V' },
};

static const char *short_opts = "shV";

static const char *usagestr =
"Usage: sqfs2tar [OPTIONS...] <sqfsfile>\n"
"\n"
"Read an input squashfs archive and turn it into a tar archive, written\n"
"to stdout.\n"
"\n"
"Possible options:\n"
"\n"
"  --no-skip                 Abort if a file cannot be stored in a tar\n"
"                            archive. By default, it is simply skipped\n"
"                            and a warning is written to stderr."
"\n"
"  --help, -h                Print help text and exit.\n"
"  --version, -V             Print version information and exit.\n"
"\n"
"Examples:\n"
"\n"
"\tsqfs2tar rootfs.sqfs > rootfs.tar\n"
"\tsqfs2tar rootfs.sqfs | gzip > rootfs.tar.gz\n"
"\tsqfs2tar rootfs.sqfs | xz > rootfs.tar.xz\n"
"\n";

static const char *filename;
static unsigned int record_counter;
static bool dont_skip = false;

static void process_args(int argc, char **argv)
{
	int i;

	for (;;) {
		i = getopt_long(argc, argv, short_opts, long_opts, NULL);
		if (i == -1)
			break;

		switch (i) {
		case 's':
			dont_skip = true;
			break;
		case 'h':
			fputs(usagestr, stdout);
			exit(EXIT_SUCCESS);
		case 'V':
			print_version();
			exit(EXIT_SUCCESS);
		default:
			goto fail_arg;
		}
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
	fputs("Try `sqfs2tar --help' for more information.\n", stderr);
	exit(EXIT_FAILURE);
}

static int terminate_archive(void)
{
	char buffer[1024];
	ssize_t ret;

	memset(buffer, '\0', sizeof(buffer));

	ret = write_retry(STDOUT_FILENO, buffer, sizeof(buffer));

	if (ret < 0) {
		perror("adding archive terminator");
		return -1;
	}

	if ((size_t)ret < sizeof(buffer)) {
		fputs("adding archive terminator: truncated write\n", stderr);
		return -1;
	}

	return 0;
}

static int write_tree_dfs(fstree_t *fs, tree_node_t *n, data_reader_t *data)
{
	char *name, *target;
	struct stat sb;
	int ret;

	if (n->parent != NULL || !S_ISDIR(n->mode)) {
		name = fstree_get_path(n);
		if (name == NULL) {
			perror("resolving tree node path");
			return -1;
		}

		assert(canonicalize_name(name) == 0);

		fstree_node_stat(fs, n, &sb);

		target = S_ISLNK(sb.st_mode) ? n->data.slink_target : NULL;
		ret = write_tar_header(STDOUT_FILENO, &sb, name, target,
				       record_counter++);

		if (ret > 0) {
			if (dont_skip) {
				fputs("Not allowed to skip files, aborting!\n",
				      stderr);
				ret = -1;
			} else {
				fprintf(stderr, "Skipping %s\n", name);
				ret = 0;
			}
			free(name);
			return ret;
		}

		free(name);

		if (ret < 0)
			return -1;

		if (S_ISREG(n->mode)) {
			if (data_reader_dump_file(data, n->data.file,
						  STDOUT_FILENO, false)) {
				return -1;
			}

			if (padd_file(STDOUT_FILENO, n->data.file->size, 512))
				return -1;
		}
	}

	if (S_ISDIR(n->mode)) {
		for (n = n->data.dir->children; n != NULL; n = n->next) {
			if (write_tree_dfs(fs, n, data))
				return -1;
		}
	}

	return 0;
}

int main(int argc, char **argv)
{
	data_reader_t *data = NULL;
	int status = EXIT_FAILURE;
	sqfs_super_t super;
	compressor_t *cmp;
	fstree_t fs;
	int sqfsfd;

	process_args(argc, argv);

	sqfsfd = open(filename, O_RDONLY);
	if (sqfsfd < 0) {
		perror(filename);
		return EXIT_FAILURE;
	}

	if (sqfs_super_read(&super, sqfsfd))
		goto out_fd;

	if (!compressor_exists(super.compression_id)) {
		fputs("Image uses a compressor that has not been built in\n",
		      stderr);
		goto out_fd;
	}

	cmp = compressor_create(super.compression_id, false,
				super.block_size, NULL);
	if (cmp == NULL)
		goto out_fd;

	if (super.flags & SQFS_FLAG_COMPRESSOR_OPTIONS) {
		if (cmp->read_options(cmp, sqfsfd))
			goto out_cmp;
	}

	if (deserialize_fstree(&fs, &super, cmp, sqfsfd, 0))
		goto out_cmp;

	data = data_reader_create(sqfsfd, &super, cmp);
	if (data == NULL)
		goto out_fs;

	if (write_tree_dfs(&fs, fs.root, data))
		goto out_data;

	if (terminate_archive())
		goto out_data;

	status = EXIT_SUCCESS;
out_data:
	data_reader_destroy(data);
out_fs:
	fstree_cleanup(&fs);
out_cmp:
	cmp->destroy(cmp);
out_fd:
	close(sqfsfd);
	return status;
}
