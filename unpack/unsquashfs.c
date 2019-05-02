/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "unsquashfs.h"

static struct option long_opts[] = {
	{ "list", required_argument, NULL, 'l' },
	{ "help", no_argument, NULL, 'h' },
	{ "version", no_argument, NULL, 'V' },
};

static const char *short_opts = "l:hV";

static const char *help_string =
"Usage: %s [OPTIONS] <squashfs-file>\n"
"\n"
"View or extract the contents of a squashfs image.\n"
"\n"
"Possible options:\n"
"  --list, -l <path>  Produce a directory listing for a given path in the\n"
"                     squashfs image.\n"
"  --help, -h         Print help text and exit.\n"
"  --version, -V      Print version information and exit.\n"
"\n";

extern const char *__progname;

static tree_node_t *find_node(tree_node_t *n, const char *path)
{
	const char *end;
	size_t len;

	while (path != NULL && *path != '\0') {
		if (!S_ISDIR(n->mode)) {
			errno = ENOTDIR;
			return NULL;
		}

		end = strchrnul(path, '/');
		len = end - path;

		for (n = n->data.dir->children; n != NULL; n = n->next) {
			if (strncmp(path, n->name, len) != 0)
				continue;
			if (n->name[len] != '\0')
				continue;
			break;
		}

		if (n == NULL) {
			errno = ENOENT;
			return NULL;
		}

		path = *end ? (end + 1) : end;
	}

	return n;
}

int main(int argc, char **argv)
{
	int i, fd, status = EXIT_FAILURE;
	char *lspath = NULL;
	sqfs_super_t super;
	compressor_t *cmp;
	fstree_t fs;

	for (;;) {
		i = getopt_long(argc, argv, short_opts, long_opts, NULL);
		if (i == -1)
			break;

		switch (i) {
		case 'l':
			if (optarg != NULL) {
				free(lspath);

				lspath = strdup(optarg ? optarg : "");
				if (lspath == NULL) {
					perror("processing arguments");
					return EXIT_FAILURE;
				}

				if (canonicalize_name(lspath)) {
					fprintf(stderr, "Invalid pth: %s\n",
						optarg);
					goto out_cmd;
				}
			}
			break;
		case 'h':
			printf(help_string, __progname);
			status = EXIT_SUCCESS;
			goto out_cmd;
		case 'V':
			print_version();
			status = EXIT_SUCCESS;
			goto out_cmd;
		default:
			goto fail_arg;
		}
	}

	if (optind >= argc) {
		fprintf(stderr, "Usage: %s [OPTIONS] <filename>\n",
			__progname);
		goto out_cmd;
	}

	fd = open(argv[optind], O_RDONLY);
	if (fd < 0) {
		perror(argv[optind]);
		goto out_cmd;
	}

	if (sqfs_super_read(&super, fd))
		goto out_fd;

	if ((super.version_major != SQFS_VERSION_MAJOR) ||
	    (super.version_minor != SQFS_VERSION_MINOR)) {
		fprintf(stderr,
			"The image uses squashfs version %d.%d\n"
			"We currently only supports version %d.%d (sorry).\n",
			super.version_major, super.version_minor,
			SQFS_VERSION_MAJOR, SQFS_VERSION_MINOR);
		goto out_fd;
	}

	if (super.flags & SQFS_FLAG_COMPRESSOR_OPTIONS) {
		fputs("Image has been built with compressor options.\n"
		      "This is not yet supported.\n",
		      stderr);
		goto out_fd;
	}

	if (!compressor_exists(super.compression_id)) {
		fputs("Image uses a compressor that has not been built in\n",
		      stderr);
		goto out_fd;
	}

	cmp = compressor_create(super.compression_id, false, super.block_size);
	if (cmp == NULL)
		goto out_fd;

	if (read_fstree(&fs, fd, &super, cmp))
		goto out_cmp;

	if (lspath != NULL) {
		tree_node_t *n = find_node(fs.root, lspath);

		if (n == NULL) {
			perror(lspath);
			goto out_fs;
		}

		list_files(n);
	}

	status = EXIT_SUCCESS;
out_fs:
	fstree_cleanup(&fs);
out_cmp:
	cmp->destroy(cmp);
out_fd:
	close(fd);
out_cmd:
	free(lspath);
	return status;
fail_arg:
	fprintf(stderr, "Try `%s --help' for more information.\n", __progname);
	free(lspath);
	return EXIT_FAILURE;
}
