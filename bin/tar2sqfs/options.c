/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * options.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "tar2sqfs.h"

static struct option long_opts[] = {
	{ "root-becomes", required_argument, NULL, 'r' },
	{ "compressor", required_argument, NULL, 'c' },
	{ "block-size", required_argument, NULL, 'b' },
	{ "dev-block-size", required_argument, NULL, 'B' },
	{ "defaults", required_argument, NULL, 'd' },
	{ "num-jobs", required_argument, NULL, 'j' },
	{ "queue-backlog", required_argument, NULL, 'Q' },
	{ "comp-extra", required_argument, NULL, 'X' },
	{ "no-skip", no_argument, NULL, 's' },
	{ "no-xattr", no_argument, NULL, 'x' },
	{ "no-keep-time", no_argument, NULL, 'k' },
	{ "exportable", no_argument, NULL, 'e' },
	{ "no-symlink-retarget", no_argument, NULL, 'S' },
	{ "no-tail-packing", no_argument, NULL, 'T' },
	{ "force", no_argument, NULL, 'f' },
	{ "quiet", no_argument, NULL, 'q' },
	{ "help", no_argument, NULL, 'h' },
	{ "version", no_argument, NULL, 'V' },
	{ NULL, 0, NULL, 0 },
};

static const char *short_opts = "r:c:b:B:d:X:j:Q:sxekfqSThV";

static const char *usagestr =
"Usage: tar2sqfs [OPTIONS...] <sqfsfile>\n"
"\n"
"Read a tar archive from stdin and turn it into a squashfs filesystem image.\n"
"\n"
"Possible options:\n"
"\n"
"  --root-becomes, -r <dir>    The specified directory becomes the root.\n"
"                              Only its children are packed into the image\n"
"                              and its attributes (ownership, permissions,\n"
"                              xattrs, ...) are stored in the root inode.\n"
"                              If not set and a tarbal has an entry for './'\n"
"                              or '/', it becomes the root instead.\n"
"  --no-symlink-retarget, -S   If --root-becomes is used, link targets are\n"
"                              adjusted if they are prefixed by the root\n"
"                              path. If this flag is set, symlinks are left\n"
"                              untouched and only hard links are changed.\n"
"\n"
"  --compressor, -c <name>     Select the compressor to use.\n"
"                              A list of available compressors is below.\n"
"  --comp-extra, -X <options>  A comma separated list of extra options for\n"
"                              the selected compressor. Specify 'help' to\n"
"                              get a list of available options.\n"
"  --num-jobs, -j <count>      Number of compressor jobs to create.\n"
"  --queue-backlog, -Q <count> Maximum number of data blocks in the thread\n"
"                              worker queue before the packer starts waiting\n"
"                              for the block processors to catch up.\n"
"                              Defaults to 10 times the number of jobs.\n"
"  --block-size, -b <size>     Block size to use for Squashfs image.\n"
"                              Defaults to %u.\n"
"  --dev-block-size, -B <size> Device block size to padd the image to.\n"
"                              Defaults to %u.\n"
"  --defaults, -d <options>    A comma separated list of default values for\n"
"                              implicitly created directories.\n"
"\n"
"                              Possible options:\n"
"                                 uid=<value>    0 if not set.\n"
"                                 gid=<value>    0 if not set.\n"
"                                 mode=<value>   0755 if not set.\n"
"                                 mtime=<value>  0 if not set.\n"
"\n"
"  --no-skip, -s               Abort if a tar record cannot be read instead\n"
"                              of skipping it.\n"
"  --no-xattr, -x              Do not copy extended attributes from archive.\n"
"  --no-keep-time, -k          Do not keep the time stamps stored in the\n"
"                              archive. Instead, set defaults on all files.\n"
"  --exportable, -e            Generate an export table for NFS support.\n"
"  --no-tail-packing, -T       Do not perform tail end packing on files that\n"
"                              are larger than block size.\n"
"  --force, -f                 Overwrite the output file if it exists.\n"
"  --quiet, -q                 Do not print out progress reports.\n"
"  --help, -h                  Print help text and exit.\n"
"  --version, -V               Print version information and exit.\n"
"\n";

bool dont_skip = false;
bool keep_time = true;
bool no_tail_pack = false;
bool no_symlink_retarget = false;
sqfs_writer_cfg_t cfg;
char *root_becomes = NULL;

static void input_compressor_print_available(void)
{
	int i = FSTREAM_COMPRESSOR_MIN;
	const char *name;

	fputs("\nSupported tar compression formats:\n", stdout);

	while (i <= FSTREAM_COMPRESSOR_MAX) {
		name = fstream_compressor_name_from_id(i);

		if (fstream_compressor_exists(i))
			printf("\t%s\n", name);

		++i;
	}

	fputs("\tuncompressed\n", stdout);
	fputc('\n', stdout);
}

void process_args(int argc, char **argv)
{
	bool have_compressor;
	int i, ret;

	sqfs_writer_cfg_init(&cfg);

	for (;;) {
		i = getopt_long(argc, argv, short_opts, long_opts, NULL);
		if (i == -1)
			break;

		switch (i) {
		case 'S':
			no_symlink_retarget = true;
			break;
		case 'T':
			no_tail_pack = true;
			break;
		case 'b':
			if (parse_size("Block size", &cfg.block_size,
				       optarg, 0)) {
				exit(EXIT_FAILURE);
			}
			break;
		case 'B':
			if (parse_size("Device block size", &cfg.devblksize,
				       optarg, 0)) {
				exit(EXIT_FAILURE);
			}
			if (cfg.devblksize < 1024) {
				fputs("Device block size must be at "
				      "least 1024\n", stderr);
				exit(EXIT_FAILURE);
			}
			break;
		case 'c':
			have_compressor = true;
			ret = sqfs_compressor_id_from_name(optarg);

			if (ret < 0) {
				have_compressor = false;
#ifdef WITH_LZO
				if (cfg.comp_id == SQFS_COMP_LZO)
					have_compressor = true;
#endif
			}

			if (!have_compressor) {
				fprintf(stderr, "Unsupported compressor '%s'\n",
					optarg);
				exit(EXIT_FAILURE);
			}

			cfg.comp_id = ret;
			break;
		case 'j':
			cfg.num_jobs = strtol(optarg, NULL, 0);
			break;
		case 'Q':
			cfg.max_backlog = strtol(optarg, NULL, 0);
			break;
		case 'X':
			cfg.comp_extra = optarg;
			break;
		case 'd':
			cfg.fs_defaults = optarg;
			break;
		case 'x':
			cfg.no_xattr = true;
			break;
		case 'k':
			keep_time = false;
			break;
		case 'r':
			free(root_becomes);
			root_becomes = strdup(optarg);
			if (root_becomes == NULL) {
				perror("copying root directory name");
				exit(EXIT_FAILURE);
			}

			if (canonicalize_name(root_becomes) != 0 ||
			    strlen(root_becomes) == 0) {
				fprintf(stderr,
					"Invalid root directory '%s'.\n",
					optarg);
				goto fail_arg;
			}
			break;
		case 's':
			dont_skip = true;
			break;
		case 'e':
			cfg.exportable = true;
			break;
		case 'f':
			cfg.outmode |= SQFS_FILE_OPEN_OVERWRITE;
			break;
		case 'q':
			cfg.quiet = true;
			break;
		case 'h':
			printf(usagestr, SQFS_DEFAULT_BLOCK_SIZE,
			       SQFS_DEVBLK_SIZE);
			compressor_print_available();
			input_compressor_print_available();
			exit(EXIT_SUCCESS);
		case 'V':
			print_version("tar2sqfs");
			exit(EXIT_SUCCESS);
		default:
			goto fail_arg;
		}
	}

	if (cfg.num_jobs < 1)
		cfg.num_jobs = 1;

	if (cfg.max_backlog < 1)
		cfg.max_backlog = 10 * cfg.num_jobs;

	if (cfg.comp_extra != NULL && strcmp(cfg.comp_extra, "help") == 0) {
		compressor_print_help(cfg.comp_id);
		exit(EXIT_SUCCESS);
	}

	if (optind >= argc) {
		fputs("Missing argument: squashfs image\n", stderr);
		goto fail_arg;
	}

	cfg.filename = argv[optind++];

	if (optind < argc) {
		fputs("Unknown extra arguments specified.\n", stderr);
		goto fail_arg;
	}
	return;
fail_arg:
	fputs("Try `tar2sqfs --help' for more information.\n", stderr);
	exit(EXIT_FAILURE);
}
