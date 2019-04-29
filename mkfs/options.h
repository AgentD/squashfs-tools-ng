/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef OPTIONS_H
#define OPTIONS_H

typedef struct {
	unsigned int def_uid;
	unsigned int def_gid;
	unsigned int def_mode;
	unsigned int def_mtime;
	int outmode;
	int compressor;
	int blksz;
	int devblksz;
	const char *infile;
	const char *outfile;
} options_t;

void process_command_line(options_t *opt, int argc, char **argv);

#endif /* OPTIONS_H */
