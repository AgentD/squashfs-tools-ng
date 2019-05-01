/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "squashfs.h"
#include "compress.h"

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>

extern const char *__progname;

int main(int argc, char **argv)
{
	int fd, status = EXIT_FAILURE;
	sqfs_super_t super;
	compressor_t *cmp;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s <filename>\n", __progname);
		return EXIT_FAILURE;
	}

	fd = open(argv[1], O_RDONLY);
	if (fd < 0) {
		perror(argv[1]);
		return EXIT_FAILURE;
	}

	if (sqfs_super_read(&super, fd))
		goto out;

	if ((super.version_major != SQFS_VERSION_MAJOR) ||
	    (super.version_minor != SQFS_VERSION_MINOR)) {
		fprintf(stderr,
			"The image uses squashfs version %d.%d\n"
			"We currently only supports version %d.%d (sorry).\n",
			super.version_major, super.version_minor,
			SQFS_VERSION_MAJOR, SQFS_VERSION_MINOR);
		goto out;
	}

	if (super.flags & SQFS_FLAG_COMPRESSOR_OPTIONS) {
		fputs("Image has been built with compressor options.\n"
		      "This is not yet supported.\n",
		      stderr);
		goto out;
	}

	if (!compressor_exists(super.compression_id)) {
		fputs("Image uses a compressor that has not been built in\n",
		      stderr);
		goto out;
	}

	cmp = compressor_create(super.compression_id, false, super.block_size);
	if (cmp == NULL)
		goto out;

	status = EXIT_SUCCESS;

	cmp->destroy(cmp);
out:
	close(fd);
	return status;
}
