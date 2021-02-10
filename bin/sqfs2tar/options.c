/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * options.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "sqfs2tar.h"

static struct option long_opts[] = {
	{ "compressor", required_argument, NULL, 'c' },
	{ "subdir", required_argument, NULL, 'd' },
	{ "keep-as-dir", no_argument, NULL, 'k' },
	{ "root-becomes", required_argument, NULL, 'r' },
	{ "no-skip", no_argument, NULL, 's' },
	{ "no-xattr", no_argument, NULL, 'X' },
	{ "no-hard-links", no_argument, NULL, 'L' },
	{ "help", no_argument, NULL, 'h' },
	{ "version", no_argument, NULL, 'V' },
	{ NULL, 0, NULL, 0 },
};

static const char *short_opts = "c:d:kr:sXLhV";

static const char *usagestr =
"Usage: sqfs2tar [OPTIONS...] <sqfsfile>\n"
"\n"
"Read an input squashfs archive and turn it into a tar archive, written\n"
"to stdout.\n"
"\n"
"Possible options:\n"
"\n"
"  --compressor, -c <name>   If set, stream compress the resulting tarball.\n"
"                            By default, the tarball is uncompressed.\n"
"\n"
"  --subdir, -d <dir>        Unpack the given sub directory instead of the\n"
"                            filesystem root. Can be specified more than\n"
"                            once to select multiple directories. If only\n"
"                            one is specified, it becomes the new root of\n"
"                            node of the archive file system tree.\n"
"\n"
"  --root-becomes, -r <dir>  Turn the root inode into a directory with the\n"
"                            specified name. Everything else will be stored\n"
"                            inside this directory. The special value '.' is\n"
"                            allowed to prefix all tar paths with './' and\n"
"                            add an entry named '.' for the root inode.\n"
"                            If this option isn't used, all meta data stored\n"
"                            in the root inode IS LOST!\n"
"\n"
"  --keep-as-dir, -k         If --subdir is used only once, don't make the\n"
"                            subdir the archive root, instead keep it as\n"
"                            prefix for all unpacked files.\n"
"                            Using --subdir more than once implies\n"
"                            --keep-as-dir.\n"
"  --no-xattr, -X            Do not copy extended attributes.\n"
"  --no-hard-links, -L       Do not generate hard links. Produce duplicate\n"
"                            entries instead.\n"
"\n"
"  --no-skip, -s             Abort if a file cannot be stored in a tar\n"
"                            archive. By default, it is simply skipped\n"
"                            and a warning is written to stderr.\n"
"\n"
"  --help, -h                Print help text and exit.\n"
"  --version, -V             Print version information and exit.\n"
"\n"
"Supported tar compression formats:\n";

bool dont_skip = false;
bool keep_as_dir = false;
bool no_xattr = false;
bool no_links = false;

char *root_becomes = NULL;
char **subdirs = NULL;
size_t num_subdirs = 0;
static size_t max_subdirs = 0;
int compressor = 0;

const char *filename = NULL;

void process_args(int argc, char **argv)
{
	size_t idx, new_count;
	const char *name;
	int i, ret;
	char **new;

	for (;;) {
		i = getopt_long(argc, argv, short_opts, long_opts, NULL);
		if (i == -1)
			break;

		switch (i) {
		case 'c':
			compressor = fstream_compressor_id_from_name(optarg);
			if (compressor <= 0) {
				fprintf(stderr, "unknown compressor '%s'.\n",
					optarg);
				goto fail;
			}

			if (!fstream_compressor_exists(compressor)) {
				fprintf(stderr,
					"%s compressor is not supported.\n",
					optarg);
				goto fail;
			}
			break;
		case 'd':
			if (num_subdirs == max_subdirs) {
				new_count = max_subdirs ? max_subdirs * 2 : 16;
				new = realloc(subdirs,
					      new_count * sizeof(subdirs[0]));
				if (new == NULL)
					goto fail_errno;

				max_subdirs = new_count;
				subdirs = new;
			}

			subdirs[num_subdirs] = strdup(optarg);
			if (subdirs[num_subdirs] == NULL)
				goto fail_errno;

			if (canonicalize_name(subdirs[num_subdirs])) {
				perror(optarg);
				goto fail;
			}

			++num_subdirs;
			break;
		case 'r':
			free(root_becomes);
			root_becomes = strdup(optarg);
			if (root_becomes == NULL)
				goto fail_errno;

			if (strcmp(root_becomes, "./") == 0)
				root_becomes[1] = '\0';

			if (strcmp(root_becomes, ".") == 0)
				break;

			if (canonicalize_name(root_becomes) != 0 ||
			    strlen(root_becomes) == 0) {
				fprintf(stderr,
					"Invalid root directory '%s'.\n",
					optarg);
				goto fail_arg;
			}
			break;
		case 'k':
			keep_as_dir = true;
			break;
		case 's':
			dont_skip = true;
			break;
		case 'X':
			no_xattr = true;
			break;
		case 'L':
			no_links = true;
			break;
		case 'h':
			fputs(usagestr, stdout);

			i = FSTREAM_COMPRESSOR_MIN;

			while (i <= FSTREAM_COMPRESSOR_MAX) {
				name = fstream_compressor_name_from_id(i);
				if (fstream_compressor_exists(i))
					printf("\t%s\n", name);
				++i;
			}

			fputc('\n', stdout);
			goto out_success;
		case 'V':
			print_version("sqfs2tar");
			goto out_success;
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

	if (num_subdirs > 1)
		keep_as_dir = true;

	return;
fail_errno:
	perror("parsing options");
	goto fail;
fail_arg:
	fputs("Try `sqfs2tar --help' for more information.\n", stderr);
	goto fail;
fail:
	ret = EXIT_FAILURE;
	goto out_exit;
out_success:
	ret = EXIT_SUCCESS;
	goto out_exit;
out_exit:
	for (idx = 0; idx < num_subdirs; ++idx)
		free(subdirs[idx]);
	free(root_becomes);
	free(subdirs);
	exit(ret);
}
