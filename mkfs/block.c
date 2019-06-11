/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "mkfs.h"
#include "util.h"

static int process_file(data_writer_t *data, file_info_t *fi)
{
	int ret, infd;

	infd = open(fi->input_file, O_RDONLY);
	if (infd < 0) {
		perror(fi->input_file);
		return -1;
	}

	ret = write_data_from_fd(data, fi, infd);

	close(infd);
	return ret;
}

static void print_name(tree_node_t *n)
{
	if (n->parent != NULL) {
		print_name(n->parent);
		fputc('/', stdout);
	}

	fputs(n->name, stdout);
}

static int find_and_process_files(data_writer_t *data, tree_node_t *n,
				  bool quiet)
{
	if (S_ISDIR(n->mode)) {
		for (n = n->data.dir->children; n != NULL; n = n->next) {
			if (find_and_process_files(data, n, quiet))
				return -1;
		}
		return 0;
	}

	if (S_ISREG(n->mode)) {
		if (!quiet) {
			fputs("packing ", stdout);
			print_name(n);
			fputc('\n', stdout);
		}

		return process_file(data, n->data.file);
	}

	return 0;
}

int write_data_to_image(data_writer_t *data, sqfs_info_t *info)
{
	bool need_restore = false;
	const char *ptr;
	int ret;

	if (info->opt.packdir != NULL) {
		if (pushd(info->opt.packdir))
			return -1;
		need_restore = true;
	} else {
		ptr = strrchr(info->opt.infile, '/');

		if (ptr != NULL) {
			if (pushdn(info->opt.infile, ptr - info->opt.infile))
				return -1;

			need_restore = true;
		}
	}

	ret = find_and_process_files(data, info->fs.root, info->opt.quiet);

	ret = ret == 0 ? data_writer_flush_fragments(data) : ret;

	if (need_restore)
		ret = popd();

	return ret;
}
