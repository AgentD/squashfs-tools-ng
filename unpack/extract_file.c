/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "unsquashfs.h"

int extract_file(file_info_t *fi, compressor_t *cmp, size_t block_size,
		 frag_reader_t *frag, int sqfsfd, int outfd)
{
	size_t i, count, fragsz;
	bool compressed;
	void *buffer;
	uint32_t bs;
	ssize_t ret;

	buffer = malloc(block_size);
	if (buffer == NULL) {
		perror("allocating scratch buffer");
		return -1;
	}

	count = fi->size / block_size;

	if (count > 0) {
		if (lseek(sqfsfd, fi->startblock, SEEK_SET) == (off_t)-1)
			goto fail_seek;

		for (i = 0; i < count; ++i) {
			bs = fi->blocksizes[i];

			compressed = (bs & (1 << 24)) == 0;
			bs &= (1 << 24) - 1;

			if (bs > block_size)
				goto fail_bs;

			ret = read_retry(sqfsfd, buffer, bs);
			if (ret < 0)
				goto fail_rd;

			if ((size_t)ret < bs)
				goto fail_trunc;

			if (compressed) {
				ret = cmp->do_block(cmp, buffer, bs);
				if (ret <= 0)
					goto fail;

				bs = ret;
			}

			ret = write_retry(outfd, buffer, bs);
			if (ret < 0)
				goto fail_wr;

			if ((size_t)ret < bs)
				goto fail_wr_trunc;
		}
	}

	fragsz = fi->size % block_size;

	if (fragsz > 0) {
		if (frag_reader_read(frag, fi->fragment, fi->fragment_offset,
				     buffer, fragsz)) {
			goto fail;
		}

		ret = write_retry(outfd, buffer, fragsz);
		if (ret < 0)
			goto fail_wr;

		if ((size_t)ret < fragsz)
			goto fail_wr_trunc;
	}

	free(buffer);
	return 0;
fail_seek:
	perror("seek on squashfs");
	goto fail;
fail_wr:
	perror("writing uncompressed block");
	goto fail;
fail_wr_trunc:
	fputs("writing uncompressed block: truncated write\n", stderr);
	goto fail;
fail_rd:
	perror("reading from squashfs");
	goto fail;
fail_trunc:
	fputs("reading from squashfs: unexpected end of file\n", stderr);
	goto fail;
fail_bs:
	fputs("found compressed block larger than block size\n", stderr);
	goto fail;
fail:
	free(buffer);
	return -1;
}
