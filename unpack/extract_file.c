/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "rdsquashfs.h"

int extract_file(file_info_t *fi, unsqfs_info_t *info, int outfd)
{
	size_t i, count, fragsz;
	bool compressed;
	uint32_t bs;
	ssize_t ret;
	void *ptr;

	count = fi->size / info->block_size;

	if (count > 0) {
		if (lseek(info->sqfsfd, fi->startblock, SEEK_SET) == (off_t)-1)
			goto fail_seek;

		for (i = 0; i < count; ++i) {
			bs = fi->blocksizes[i];

			compressed = (bs & (1 << 24)) == 0;
			bs &= (1 << 24) - 1;

			if (bs > info->block_size)
				goto fail_bs;

			ret = read_retry(info->sqfsfd, info->buffer, bs);
			if (ret < 0)
				goto fail_rd;

			if ((size_t)ret < bs)
				goto fail_trunc;

			if (compressed) {
				ret = info->cmp->do_block(info->cmp,
							  info->buffer, bs,
							  info->scratch,
							  info->block_size);
				if (ret <= 0)
					return -1;

				bs = ret;
				ptr = info->scratch;
			} else {
				ptr = info->buffer;
			}

			ret = write_retry(outfd, ptr, bs);
			if (ret < 0)
				goto fail_wr;

			if ((size_t)ret < bs)
				goto fail_wr_trunc;
		}
	}

	fragsz = fi->size % info->block_size;

	if (fragsz > 0) {
		if (frag_reader_read(info->frag, fi->fragment,
				     fi->fragment_offset, info->buffer,
				     fragsz)) {
			return -1;
		}

		ret = write_retry(outfd, info->buffer, fragsz);
		if (ret < 0)
			goto fail_wr;

		if ((size_t)ret < fragsz)
			goto fail_wr_trunc;
	}

	return 0;
fail_seek:
	perror("seek on squashfs");
	return -1;
fail_wr:
	perror("writing uncompressed block");
	return -1;
fail_wr_trunc:
	fputs("writing uncompressed block: truncated write\n", stderr);
	return -1;
fail_rd:
	perror("reading from squashfs");
	return -1;
fail_trunc:
	fputs("reading from squashfs: unexpected end of file\n", stderr);
	return -1;
fail_bs:
	fputs("found compressed block larger than block size\n", stderr);
	return -1;
}
