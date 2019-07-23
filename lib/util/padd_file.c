/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"

#include "util.h"

#include <stdlib.h>
#include <stdio.h>

int padd_file(int outfd, uint64_t size, size_t blocksize)
{
	size_t padd_sz = size % blocksize;
	int status = -1;
	uint8_t *buffer;

	if (padd_sz == 0)
		return 0;

	padd_sz = blocksize - padd_sz;

	buffer = calloc(1, padd_sz);
	if (buffer == NULL)
		goto fail_errno;

	if (write_data("padding output file to block size",
		       outfd, buffer, padd_sz)) {
		goto out;
	}

	status = 0;
out:
	free(buffer);
	return status;
fail_errno:
	perror("padding output file to block size");
	goto out;
}
