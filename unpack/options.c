/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "rdsquashfs.h"

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
	{ "no-sparse", no_argument, NULL, 'Z' },
	{ "describe", no_argument, NULL, 'd' },
	{ "chmod", no_argument, NULL, 'C' },
	{ "chown", no_argument, NULL, 'O' },
	{ "quiet", no_argument, NULL, 'q' },
	{ "help", no_argument, NULL, 'h' },
	{ "version", no_argument, NULL, 'V' },
};

static const char *short_opts = "l:c:u:p:DSFLCOEZdqhV";

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
"  --no-sparse, -Z           Do not create sparse files, always write zero\n"
"                            blocks to disk.\n"
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

void process_command_line(options_t *opt, int argc, char **argv)
{
	int i;

	opt->op = OP_NONE;
	opt->rdtree_flags = 0;
	opt->flags = 0;
	opt->cmdpath = NULL;
	opt->unpack_root = NULL;
	opt->image_name = NULL;

	for (;;) {
		i = getopt_long(argc, argv, short_opts, long_opts, NULL);
		if (i == -1)
			break;

		switch (i) {
		case 'D':
			opt->rdtree_flags |= RDTREE_NO_DEVICES;
			break;
		case 'S':
			opt->rdtree_flags |= RDTREE_NO_SOCKETS;
			break;
		case 'F':
			opt->rdtree_flags |= RDTREE_NO_FIFO;
			break;
		case 'L':
			opt->rdtree_flags |= RDTREE_NO_SLINKS;
			break;
		case 'E':
			opt->rdtree_flags |= RDTREE_NO_EMPTY;
			break;
		case 'C':
			opt->flags |= UNPACK_CHMOD;
			break;
		case 'O':
			opt->flags |= UNPACK_CHOWN;
			break;
		case 'Z':
			opt->flags |= UNPACK_NO_SPARSE;
			break;
		case 'c':
			opt->op = OP_CAT;
			opt->cmdpath = get_path(opt->cmdpath, optarg);
			break;
		case 'd':
			opt->op = OP_DESCRIBE;
			free(opt->cmdpath);
			opt->cmdpath = NULL;
			break;
		case 'l':
			opt->op = OP_LS;
			opt->cmdpath = get_path(opt->cmdpath, optarg);
			break;
		case 'p':
			opt->unpack_root = optarg;
			break;
		case 'u':
			opt->op = OP_UNPACK;
			opt->cmdpath = get_path(opt->cmdpath, optarg);
			break;
		case 'q':
			opt->flags |= UNPACK_QUIET;
			break;
		case 'h':
			printf(help_string, __progname);
			free(opt->cmdpath);
			exit(EXIT_SUCCESS);
		case 'V':
			print_version();
			free(opt->cmdpath);
			exit(EXIT_SUCCESS);
		default:
			goto fail_arg;
		}
	}

	if (opt->op == OP_NONE) {
		fputs("No opteration specified\n", stderr);
		goto fail_arg;
	}

	if (optind >= argc) {
		fputs("Missing image argument\n", stderr);
		goto fail_arg;
	}

	opt->image_name = argv[optind++];
	return;
fail_arg:
	fprintf(stderr, "Try `%s --help' for more information.\n", __progname);
	free(opt->cmdpath);
	exit(EXIT_FAILURE);
}
