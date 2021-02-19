/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * options.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "mkfs.h"

enum {
	ALL_ROOT_OPTION = 1,
};

static struct option long_opts[] = {
	{ "all-root", no_argument, NULL, ALL_ROOT_OPTION },
	{ "set-uid", required_argument, NULL, 'u' },
	{ "set-gid", required_argument, NULL, 'g' },
	{ "compressor", required_argument, NULL, 'c' },
	{ "block-size", required_argument, NULL, 'b' },
	{ "dev-block-size", required_argument, NULL, 'B' },
	{ "defaults", required_argument, NULL, 'd' },
	{ "comp-extra", required_argument, NULL, 'X' },
	{ "pack-file", required_argument, NULL, 'F' },
	{ "pack-dir", required_argument, NULL, 'D' },
	{ "num-jobs", required_argument, NULL, 'j' },
	{ "queue-backlog", required_argument, NULL, 'Q' },
	{ "keep-time", no_argument, NULL, 'k' },
#ifdef HAVE_SYS_XATTR_H
	{ "keep-xattr", no_argument, NULL, 'x' },
#endif
	{ "one-file-system", no_argument, NULL, 'o' },
	{ "exportable", no_argument, NULL, 'e' },
	{ "no-tail-packing", no_argument, NULL, 'T' },
	{ "force", no_argument, NULL, 'f' },
	{ "quiet", no_argument, NULL, 'q' },
#ifdef WITH_SELINUX
	{ "selinux", required_argument, NULL, 's' },
#endif
	{ "version", no_argument, NULL, 'V' },
	{ "help", no_argument, NULL, 'h' },
	{ NULL, 0, NULL, 0 },
};

static const char *short_opts = "F:D:X:c:b:B:d:u:g:j:Q:kxoefqThV"
#ifdef WITH_SELINUX
"s:"
#endif
#ifdef HAVE_SYS_XATTR_H
"x"
#endif
;

static const char *help_string =
"Usage: gensquashfs [OPTIONS...] <squashfs-file>\n"
"\n"
"Possible options:\n"
"\n"
"  --pack-file, -F <file>      Use a `gen_init_cpio` style description file.\n"
"                              The file format is specified below.\n"
"                              If --pack-dir is used, input file paths are\n"
"                              relative to the pack directory, otherwise\n"
"                              they are relative to the directory the pack\n"
"                              file is in.\n"
"  --pack-dir, -D <directory>  If --pack-file is used, this is the root path\n"
"                              relative to which to read files. If no pack\n"
"                              file is specified, pack the contents of the\n"
"                              given directory into a SquashFS image. The\n"
"                              directory becomes the root of the file\n"
"                              system.\n"
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
"  --set-uid, -u <number>      Force the owners user ID for ALL inodes to\n"
"                              this value, no matter what the pack file or\n"
"                              directory entries actually specify.\n"
"  --set-gid, -g <number>      Force the owners group ID for ALL inodes to\n"
"                              this value, no matter what the pack file or\n"
"                              directory entries actually specify.\n"
"  --all-root                  A short hand for `--set-uid 0 --set-gid 0`.\n"
"\n"
#ifdef WITH_SELINUX
"  --selinux, -s <file>        Specify an SELinux label file to get context\n"
"                              attributes from.\n"
#endif
"  --keep-time, -k             When using --pack-dir only, use the timestamps\n"
"                              from the input files instead of setting\n"
"                              defaults on all input paths.\n"
"  --keep-xattr, -x            When using --pack-dir only, read and pack the\n"
"                              extended attributes from the input files.\n"
"  --one-file-system, -o       When using --pack-dir only, stay in local file\n"
"                              system and do not cross mount points.\n"
"  --exportable, -e            Generate an export table for NFS support.\n"
"  --no-tail-packing, -T       Do not perform tail end packing on files that\n"
"                              are larger than block size.\n"
"  --force, -f                 Overwrite the output file if it exists.\n"
"  --quiet, -q                 Do not print out progress reports.\n"
"  --help, -h                  Print help text and exit.\n"
"  --version, -V               Print version information and exit.\n"
"\n";

const char *help_details =
"When using the pack file option, the given file is expected to contain\n"
"newline separated entries that describe the files to be included in the\n"
"SquashFS image. The following entry types can be specified:\n"
"\n"
"# a comment\n"
"file <path> <mode> <uid> <gid> [<location>]\n"
"dir <path> <mode> <uid> <gid>\n"
"nod <path> <mode> <uid> <gid> <dev_type> <maj> <min>\n"
"slink <path> <mode> <uid> <gid> <target>\n"
"link <path> <dummy> <dummy> <dummy> <target>\n"
"pipe <path> <mode> <uid> <gid>\n"
"sock <path> <mode> <uid> <gid>\n"
"glob <path> <mode|*> <uid|*> <gid|*> [OPTIONS...] <location>\n"
"\n"
"<path>       Absolute path of the entry in the image. Can be put in quotes\n"
"             if some components contain spaces.\n"
"<location>   If given, location of the input file. Either absolute or relative\n"
"             to the description file. If omitted, the image path is used,\n"
"             relative to the description file.\n"
"<target>     Symlink or hardlink target.\n"
"<mode>       Mode/permissions of the entry.\n"
"<uid>        Numeric user id.\n"
"<gid>        Numeric group id.\n"
"<dev_type>   Device type (b=block, c=character).\n"
"<maj>        Major number of a device special file.\n"
"<min>        Minor number of a device special file.\n"
"\n"
"Example:\n"
"    # A simple squashfs image\n"
"    dir /dev 0755 0 0\n"
"    nod /dev/console 0600 0 0 c 5 1\n"
"    dir /root 0700 0 0\n"
"    dir /sbin 0755 0 0\n"
"    \n"
"    # Add a file. Input is relative to listing or pack dir.\n"
"    file /sbin/init 0755 0 0 ../init/sbin/init\n"
"    \n"
"    # Read bin/bash, relative to listing or pack dir.\n"
"    # Implicitly create /bin.\n"
"    file /bin/bash 0755 0 0\n"
"    \n"
"    # file name with a space in it.\n"
"    file \"/opt/my app/\\\"special\\\"/data\" 0600 0 0\n"
"    \n"
"    # collect the contents of ./lib and put it under /usr/lib\n"
"    glob /usr/lib 0755 0 0 -type d ./lib\n"
"    glob /usr/lib 0755 0 0 -type f -name \"*.so.*\" ./lib\n"
"    glob /usr/lib 0777 0 0 -type l -name \"*.so.*\" ./lib\n"
"\n\n";

void process_command_line(options_t *opt, int argc, char **argv)
{
	bool have_compressor;
	int i, ret;

	memset(opt, 0, sizeof(*opt));
	sqfs_writer_cfg_init(&opt->cfg);

	for (;;) {
		i = getopt_long(argc, argv, short_opts, long_opts, NULL);
		if (i == -1)
			break;

		switch (i) {
		case ALL_ROOT_OPTION:
			opt->force_uid_value = 0;
			opt->force_gid_value = 0;
			opt->force_uid = true;
			opt->force_gid = true;
			break;
		case 'u':
			opt->force_uid_value = strtol(optarg, NULL, 0);
			opt->force_uid = true;
			break;
		case 'g':
			opt->force_gid_value = strtol(optarg, NULL, 0);
			opt->force_gid = true;
			break;
		case 'T':
			opt->no_tail_packing = true;
			break;
		case 'c':
			have_compressor = true;
			ret = sqfs_compressor_id_from_name(optarg);

			if (ret < 0) {
				have_compressor = false;
#ifdef WITH_LZO
				if (opt->cfg.comp_id == SQFS_COMP_LZO)
					have_compressor = true;
#endif
			}

			if (!have_compressor) {
				fprintf(stderr, "Unsupported compressor '%s'\n",
					optarg);
				exit(EXIT_FAILURE);
			}

			opt->cfg.comp_id = ret;
			break;
		case 'b':
			if (parse_size("Block size", &opt->cfg.block_size,
				       optarg, 0)) {
				exit(EXIT_FAILURE);
			}
			break;
		case 'j':
			opt->cfg.num_jobs = strtol(optarg, NULL, 0);
			break;
		case 'Q':
			opt->cfg.max_backlog = strtol(optarg, NULL, 0);
			break;
		case 'B':
			if (parse_size("Device block size",
				       &opt->cfg.devblksize, optarg, 0)) {
				exit(EXIT_FAILURE);
			}
			if (opt->cfg.devblksize < 1024) {
				fputs("Device block size must be at "
				      "least 1024\n", stderr);
				exit(EXIT_FAILURE);
			}
			break;
		case 'd':
			opt->cfg.fs_defaults = optarg;
			break;
		case 'k':
			opt->dirscan_flags |= DIR_SCAN_KEEP_TIME;
			break;
#ifdef HAVE_SYS_XATTR_H
		case 'x':
			opt->scan_xattr = true;
			break;
#endif
		case 'o':
			opt->dirscan_flags |= DIR_SCAN_ONE_FILESYSTEM;
			break;
		case 'e':
			opt->cfg.exportable = true;
			break;
		case 'f':
			opt->cfg.outmode |= SQFS_FILE_OPEN_OVERWRITE;
			break;
		case 'q':
			opt->cfg.quiet = true;
			break;
		case 'X':
			opt->cfg.comp_extra = optarg;
			break;
		case 'F':
			opt->infile = optarg;
			break;
		case 'D':
			free(opt->packdir);
			opt->packdir = strdup(optarg);
			if (opt->packdir == NULL) {
				perror(optarg);
				exit(EXIT_FAILURE);
			}
			break;
#ifdef WITH_SELINUX
		case 's':
			opt->selinux = optarg;
			break;
#endif
		case 'h':
			printf(help_string,
			       SQFS_DEFAULT_BLOCK_SIZE, SQFS_DEVBLK_SIZE);
			fputs(help_details, stdout);
			compressor_print_available();
			exit(EXIT_SUCCESS);
		case 'V':
			print_version("gensquashfs");
			exit(EXIT_SUCCESS);
		default:
			goto fail_arg;
		}
	}

	if (opt->cfg.num_jobs < 1)
		opt->cfg.num_jobs = 1;

	if (opt->cfg.max_backlog < 1)
		opt->cfg.max_backlog = 10 * opt->cfg.num_jobs;

	if (opt->cfg.comp_extra != NULL &&
	    strcmp(opt->cfg.comp_extra, "help") == 0) {
		compressor_print_help(opt->cfg.comp_id);
		exit(EXIT_SUCCESS);
	}

	if (opt->infile == NULL && opt->packdir == NULL) {
		fputs("No input file or directory specified.\n", stderr);
		goto fail_arg;
	}

	if (optind >= argc) {
		fputs("No output file specified.\n", stderr);
		goto fail_arg;
	}

	opt->cfg.filename = argv[optind++];

	if (optind < argc) {
		fputs("Unknown extra arguments specified.\n", stderr);
		goto fail_arg;
	}

	/* construct packdir if not specified */
	if (opt->packdir == NULL && opt->infile != NULL) {
		const char *split = strrchr(opt->infile, '/');

		if (split != NULL) {
			opt->packdir = strndup(opt->infile,
					       split - opt->infile);

			if (opt->packdir == NULL) {
				perror("constructing input directory path");
				exit(EXIT_FAILURE);
			}
		}
	}
	return;
fail_arg:
	fputs("Try `gensquashfs --help' for more information.\n", stderr);
	free(opt->packdir);
	exit(EXIT_FAILURE);
}
