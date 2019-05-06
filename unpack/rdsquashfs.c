/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "rdsquashfs.h"

enum {
	OP_NONE = 0,
	OP_LS,
	OP_CAT,
	OP_UNPACK,
	OP_DESCRIBE,
};

static struct option long_opts[] = {
	{ "list", required_argument, NULL, 'l' },
	{ "cat", required_argument, NULL, 'c' },
	{ "unpack-root", required_argument, NULL, 'p' },
	{ "unpack-path", required_argument, NULL, 'u' },
	{ "no-dev", no_argument, NULL, 'D' },
	{ "no-sock", no_argument, NULL, 'S' },
	{ "no-fifo", no_argument, NULL, 'F' },
	{ "no-slink", no_argument, NULL, 'L' },
	{ "no-empty-dir", no_argument, NULL, 'E' },
	{ "describe", no_argument, NULL, 'd' },
	{ "chmod", no_argument, NULL, 'C' },
	{ "chown", no_argument, NULL, 'O' },
	{ "quiet", no_argument, NULL, 'q' },
	{ "help", no_argument, NULL, 'h' },
	{ "version", no_argument, NULL, 'V' },
};

static const char *short_opts = "l:c:u:p:DSFLCOEdqhV";

static const char *help_string =
"Usage: %s [OPTIONS] <squashfs-file>\n"
"\n"
"View or extract the contents of a squashfs image.\n"
"\n"
"Possible options:\n"
"\n"
"  --list, -l <path>         Produce a directory listing for a given path in\n"
"                            the squashfs image.\n"
"  --cat, -c <path>          If the specified path is a regular file in the,\n"
"                            image, dump its contents to stdout.\n"
"  --unpack-path, -u <path>  Unpack this sub directory from the image. To\n"
"                            unpack everything, simply specify /.\n"
"  --describe, -d            Produce a file listing from the image.\n"
"\n"
"  --unpack-root, -p <path>  If used with --unpack-path, this is where the\n"
"                            data unpacked to. If used with --describe, this\n"
"                            is used as a prefix for the input path of\n"
"                            regular files.\n"
"\n"
"  --no-dev, -D              Do not unpack device special files.\n"
"  --no-sock, -S             Do not unpack socket files.\n"
"  --no-fifo, -F             Do not unpack named pipes.\n"
"  --no-slink, -L            Do not unpack symbolic links.\n"
"  --no-empty-dir, -E        Do not unpack directories that would end up\n"
"                            empty after applying the above rules.\n"
"  --chmod, -C               Change permission flags of unpacked files to\n"
"                            those store in the squashfs image.\n"
"  --chown, -O               Change ownership of unpacked files to the\n"
"                            UID/GID set in the squashfs image.\n"
"  --quiet, -q               Do not print out progress while unpacking.\n"
"\n"
"  --help, -h                Print help text and exit.\n"
"  --version, -V             Print version information and exit.\n"
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

static char *get_path(char *old, const char *arg)
{
	char *path;

	free(old);

	path = strdup(arg);
	if (path == NULL) {
		perror("processing arguments");
		exit(EXIT_FAILURE);
	}

	if (canonicalize_name(path)) {
		fprintf(stderr, "Invalid path: %s\n", arg);
		free(path);
		exit(EXIT_FAILURE);
	}

	return path;
}

static int alloc_buffers(unsqfs_info_t *info, sqfs_super_t *super)
{
	info->buffer = malloc(super->block_size);
	if (info->buffer == NULL) {
		perror("allocating block buffer");
		return -1;
	}

	info->scratch = malloc(super->block_size);
	if (info->scratch == NULL) {
		perror("allocating scrtach buffer for extracting blocks");
		return -1;
	}

	return 0;
}

static int load_fragment_table(unsqfs_info_t *info, sqfs_super_t *super)
{
	if (super->fragment_entry_count == 0)
		return 0;

	if (super->flags & SQFS_FLAG_NO_FRAGMENTS)
		return 0;

	if (super->fragment_table_start >= super->bytes_used) {
		fputs("Fragment table start is past end of file\n", stderr);
		return -1;
	}

	info->frag = frag_reader_create(super, info->sqfsfd, info->cmp);
	if (info->frag == NULL)
		return -1;

	return 0;
}

int main(int argc, char **argv)
{
	int i, status = EXIT_FAILURE, op = OP_NONE;
	const char *unpack_root = NULL;
	char *cmdpath = NULL;
	unsqfs_info_t info;
	sqfs_super_t super;
	tree_node_t *n;
	fstree_t fs;

	memset(&info, 0, sizeof(info));

	for (;;) {
		i = getopt_long(argc, argv, short_opts, long_opts, NULL);
		if (i == -1)
			break;

		switch (i) {
		case 'D':
			info.flags |= UNPACK_NO_DEVICES;
			break;
		case 'S':
			info.flags |= UNPACK_NO_SOCKETS;
			break;
		case 'F':
			info.flags |= UNPACK_NO_FIFO;
			break;
		case 'L':
			info.flags |= UNPACK_NO_SLINKS;
			break;
		case 'C':
			info.flags |= UNPACK_CHMOD;
			break;
		case 'O':
			info.flags |= UNPACK_CHOWN;
			break;
		case 'E':
			info.flags |= UNPACK_NO_EMPTY;
			break;
		case 'c':
			op = OP_CAT;
			cmdpath = get_path(cmdpath, optarg);
			break;
		case 'd':
			op = OP_DESCRIBE;
			break;
		case 'l':
			op = OP_LS;
			cmdpath = get_path(cmdpath, optarg);
			break;
		case 'p':
			unpack_root = optarg;
			break;
		case 'u':
			op = OP_UNPACK;
			cmdpath = get_path(cmdpath, optarg);
			break;
		case 'q':
			info.flags |= UNPACK_QUIET;
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

	if (op == OP_NONE) {
		fputs("No opteration specified\n", stderr);
		goto fail_arg;
	}

	if (optind >= argc) {
		fputs("Missing image argument\n", stderr);
		goto fail_arg;
	}

	info.sqfsfd = open(argv[optind], O_RDONLY);
	if (info.sqfsfd < 0) {
		perror(argv[optind]);
		goto out_cmd;
	}

	if (sqfs_super_read(&super, info.sqfsfd))
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

	if (!compressor_exists(super.compression_id)) {
		fputs("Image uses a compressor that has not been built in\n",
		      stderr);
		goto out_fd;
	}

	info.cmp = compressor_create(super.compression_id, false,
				     super.block_size);
	if (info.cmp == NULL)
		goto out_fd;

	if (super.flags & SQFS_FLAG_COMPRESSOR_OPTIONS) {
		if (info.cmp->read_options(info.cmp, info.sqfsfd))
			goto out_cmp;
	}

	if (read_fstree(&fs, &super, &info))
		goto out_cmp;

	info.block_size = super.block_size;

	switch (op) {
	case OP_LS:
		n = find_node(fs.root, cmdpath);
		if (n == NULL) {
			perror(cmdpath);
			goto out_fs;
		}

		list_files(n);
		break;
	case OP_CAT:
		n = find_node(fs.root, cmdpath);
		if (n == NULL) {
			perror(cmdpath);
			goto out_fs;
		}

		if (!S_ISREG(n->mode)) {
			fprintf(stderr, "/%s: not a regular file\n", cmdpath);
			goto out_fs;
		}

		if (load_fragment_table(&info, &super))
			goto out_fs;

		if (alloc_buffers(&info, &super))
			goto out_fs;

		if (extract_file(n->data.file, &info, STDOUT_FILENO))
			goto out_fs;
		break;
	case OP_UNPACK:
		n = find_node(fs.root, cmdpath);
		if (n == NULL) {
			perror(cmdpath);
			goto out_fs;
		}

		if (load_fragment_table(&info, &super))
			goto out_fs;

		if (alloc_buffers(&info, &super))
			goto out_fs;

		if (restore_fstree(unpack_root, n, &info))
			goto out_fs;
		break;
	case OP_DESCRIBE:
		describe_tree(fs.root, unpack_root);
		break;
	}

	status = EXIT_SUCCESS;
out_fs:
	if (info.frag != NULL)
		frag_reader_destroy(info.frag);
	fstree_cleanup(&fs);
out_cmp:
	info.cmp->destroy(info.cmp);
out_fd:
	close(info.sqfsfd);
out_cmd:
	free(cmdpath);
	free(info.buffer);
	free(info.scratch);
	return status;
fail_arg:
	fprintf(stderr, "Try `%s --help' for more information.\n", __progname);
	free(cmdpath);
	return EXIT_FAILURE;
}
