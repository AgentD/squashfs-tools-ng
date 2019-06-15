/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "util.h"

#include <stdlib.h>
#include <stdio.h>

int padd_file(int outfd, uint64_t size, size_t blocksize)
{
	size_t padd_sz = size % blocksize;
	int status = -1;
	uint8_t *buffer;
	ssize_t ret;

	if (padd_sz == 0)
		return 0;

	padd_sz = blocksize - padd_sz;

	buffer = calloc(1, padd_sz);
	if (buffer == NULL)
		goto fail_errno;

	ret = write_retry(outfd, buffer, padd_sz);

	if (ret < 0)
		goto fail_errno;

	if ((size_t)ret < padd_sz)
		goto fail_trunc;

	status = 0;
out:
	free(buffer);
	return status;
fail_trunc:
	fputs("padding output to block size: truncated write\n", stderr);
	goto out;
fail_errno:
	perror("padding output file to block size");
	goto out;
}
