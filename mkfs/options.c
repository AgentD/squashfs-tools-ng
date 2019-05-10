/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "mkfs.h"
#include "util.h"

#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdio.h>

static struct option long_opts[] = {
	{ "compressor", required_argument, NULL, 'c' },
	{ "block-size", required_argument, NULL, 'b' },
	{ "dev-block-size", required_argument, NULL, 'B' },
	{ "defaults", required_argument, NULL, 'd' },
	{ "force", no_argument, NULL, 'f' },
	{ "quiet", no_argument, NULL, 'q' },
#ifdef WITH_SELINUX
	{ "selinux", required_argument, NULL, 's' },
#endif
	{ "version", no_argument, NULL, 'V' },
	{ "help", no_argument, NULL, 'h' },
};

#ifdef WITH_SELINUX
static const char *short_opts = "s:c:b:B:d:fqhV";
#else
static const char *short_opts = "c:b:B:d:fqhV";
#endif

enum {
	DEF_UID = 0,
	DEF_GID,
	DEF_MODE,
	DEF_MTIME,
};

static const char *defaults[] = {
	[DEF_UID] = "uid",
	[DEF_GID] = "gid",
	[DEF_MODE] = "mode",
	[DEF_MTIME] = "mtime",
	NULL
};

extern char *__progname;

static const char *help_string =
"Usage: %s [OPTIONS] <file-list> <squashfs-file>\n"
"\n"
"<file-list> is a file containing newline separated entries that describe\n"
"the files to be included in the squashfs image:\n"
"\n"
"# a comment\n"
"file <path> <mode> <uid> <gid> [<location>]\n"
"dir <path> <mode> <uid> <gid>\n"
"nod <path> <mode> <uid> <gid> <dev_type> <maj> <min>\n"
"slink <path> <mode> <uid> <gid> <target>\n"
"pipe <path> <mode> <uid> <gid>\n"
"sock <path> <mode> <uid> <gid>\n"
"\n"
"<path>       Absolute path of the entry in the image.\n"
"<location>   If given, location of the input file. Either absolute or relative\n"
"             to the description file. If omitted, the image path is used,\n"
"             relative to the description file.\n"
"<target>     Symlink target.\n"
"<mode>       Mode/permissions of the entry.\n"
"<uid>        Numeric user id.\n"
"<gid>        Numeric group id.\n"
"<dev_type>   Device type (b=block, c=character).\n"
"<maj>        Major number of a device special file.\n"
"<min>        Minor number of a device special file.\n"
"\n"
"Example:\n"
"# A simple squashfs image\n"
"dir /dev 0755 0 0\n"
"nod /dev/console 0600 0 0 c 5 1\n"
"dir /root 0700 0 0\n"
"dir /sbin 0755 0 0\n"
"\n"
"# Add a file. Input is relative to this listing.\n"
"file /sbin/init 0755 0 0 ../init/sbin/init\n"
"\n"
"# Read from ./bin/bash. /bin is created implicitly with default attributes.\n"
"file /bin/bash 0755 0 0"
"\n"
"Possible options:\n"
"\n"
"  --compressor, -c <name>     Select the compressor to use.\n"
"                              directories (defaults to 'xz').\n"
"  --block-size, -b <size>     Block size to use for Squashfs image.\n"
"                              Defaults to %u.\n"
"  --dev-block-size, -B <size> Device block size to padd the image to.\n"
"                              Defaults to %u.\n"
"  --defaults, -d <options>    A comma seperated list of default values for\n"
"                              implicitly created directories.\n"
"\n"
"                              Possible options:\n"
"                                 uid=<value>    0 if not set.\n"
"                                 gid=<value>    0 if not set.\n"
"                                 mode=<value>   0755 if not set.\n"
"                                 mtime=<value>  0 if not set.\n"
"\n"
#ifdef WITH_SELINUX
"  --selinux, s <file>         Specify an SELinux label file to get context\n"
"                              attributes from.\n"
#endif
"  --force, -f                 Overwrite the output file if it exists.\n"
"  --quiet, -q                 Do not print out progress reports.\n"
"  --help, -h                  Print help text and exit.\n"
"  --version, -V               Print version information and exit.\n"
"\n";

static const char *compressors[] = {
	[SQFS_COMP_GZIP] = "gzip",
	[SQFS_COMP_LZMA] = "lzma",
	[SQFS_COMP_LZO] = "lzo",
	[SQFS_COMP_XZ] = "xz",
	[SQFS_COMP_LZ4] = "lz4",
	[SQFS_COMP_ZSTD] = "zstd",
};

static long read_number(const char *name, const char *str, long min, long max)
{
	long base = 10, result = 0;
	int x;

	if (str[0] == '0') {
		if (str[1] == 'x' || str[1] == 'X') {
			base = 16;
			str += 2;
		} else {
			base = 8;
		}
	}

	if (!isxdigit(*str))
		goto fail_num;

	while (isxdigit(*str)) {
		x = *(str++);

		if (isupper(x)) {
			x = x - 'A' + 10;
		} else if (islower(x)) {
			x = x - 'a' + 10;
		} else {
			x -= '0';
		}

		if (x >= base)
			goto fail_num;

		if (result > (LONG_MAX - x) / base)
			goto fail_ov;

		result = result * base + x;
	}

	if (result < min)
		goto fail_uf;

	if (result > max)
		goto fail_ov;

	return result;
fail_num:
	fprintf(stderr, "%s: expected numeric value > 0\n", name);
	goto fail;
fail_uf:
	fprintf(stderr, "%s: number to small\n", name);
	goto fail;
fail_ov:
	fprintf(stderr, "%s: number to large\n", name);
	goto fail;
fail:
	exit(EXIT_FAILURE);
}

static void process_defaults(options_t *opt, char *subopts)
{
	char *value;
	int i;

	while (*subopts != '\0') {
		i = getsubopt(&subopts, (char *const *)defaults, &value);

		if (value == NULL) {
			fprintf(stderr, "Missing value for option %s\n",
				defaults[i]);
			exit(EXIT_FAILURE);
		}

		switch (i) {
		case DEF_UID:
			opt->def_uid = read_number("Default user ID", value,
						   0, 0xFFFFFFFF);
			break;
		case DEF_GID:
			opt->def_gid = read_number("Default group ID", value,
						   0, 0xFFFFFFFF);
			break;
		case DEF_MODE:
			opt->def_mode = read_number("Default permissions",
						    value, 0, 0xFFFFFFFF);
			break;
		case DEF_MTIME:
			opt->def_mtime = read_number("Default mtime", value,
						     0, 0xFFFFFFFF);
			break;
		default:
			fprintf(stderr, "Unknown option '%s'\n", value);
			exit(EXIT_FAILURE);
		}
	}
}

void process_command_line(options_t *opt, int argc, char **argv)
{
	bool have_compressor;
	int i;

	opt->def_uid = 0;
	opt->def_gid = 0;
	opt->def_mode = 0755;
	opt->def_mtime = 0;
	opt->outmode = O_WRONLY | O_CREAT | O_EXCL;
	opt->compressor = SQFS_COMP_XZ;
	opt->blksz = SQFS_DEFAULT_BLOCK_SIZE;
	opt->devblksz = SQFS_DEVBLK_SIZE;
	opt->infile = NULL;
	opt->outfile = NULL;

	for (;;) {
		i = getopt_long(argc, argv, short_opts, long_opts, NULL);
		if (i == -1)
			break;

		switch (i) {
		case 'c':
			have_compressor = false;

			for (i = SQFS_COMP_MIN; i <= SQFS_COMP_MAX; ++i) {
				if (strcmp(compressors[i], optarg) == 0) {
					if (compressor_exists(i)) {
						have_compressor = true;
						opt->compressor = i;
						break;
					}
				}
			}

			if (!have_compressor) {
				fprintf(stderr, "Unsupported compressor '%s'\n",
					optarg);
				exit(EXIT_FAILURE);
			}
			break;
		case 'b':
			opt->blksz = read_number("Block size", optarg,
						 4096, (1 << 20) - 1);
			break;
		case 'B':
			opt->devblksz = read_number("Device block size", optarg,
						    4096, 0xFFFFFFFF);
			break;
		case 'd':
			process_defaults(opt, optarg);
			break;
		case 'f':
			opt->outmode = O_WRONLY | O_CREAT | O_TRUNC;
			break;
		case 'q':
			opt->quiet = true;
			break;
#ifdef WITH_SELINUX
		case 's':
			opt->selinux = optarg;
			break;
#endif
		case 'h':
			printf(help_string, __progname,
			       SQFS_DEFAULT_BLOCK_SIZE, SQFS_DEVBLK_SIZE);

			fputs("Available compressors:\n", stdout);

			for (i = SQFS_COMP_MIN; i <= SQFS_COMP_MAX; ++i) {
				if (compressor_exists(i))
					printf("\t%s\n", compressors[i]);
			}

			exit(EXIT_SUCCESS);
		case 'V':
			print_version();
			exit(EXIT_SUCCESS);
		default:
			goto fail_arg;
		}
	}

	if ((optind + 1) >= argc) {
		fputs("Missing arguments: input and output files.\n", stderr);
		goto fail_arg;
	}

	opt->infile = argv[optind++];
	opt->outfile = argv[optind++];
	return;
fail_arg:
	fprintf(stderr, "Try `%s --help' for more information.\n", __progname);
	exit(EXIT_FAILURE);
}
