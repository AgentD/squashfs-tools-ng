/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "mksquashfs.h"
#include "util.h"

static int write_block(file_info_t *fi, sqfs_info_t *info)
{
	size_t idx, bs;
	ssize_t ret;

	idx = info->file_block_count++;
	bs = info->super.block_size;

	ret = info->cmp->do_block(info->cmp, info->block, bs);
	if (ret < 0)
		return -1;

	if (ret > 0) {
		bs = ret;
		fi->blocksizes[idx] = bs;
	} else {
		fi->blocksizes[idx] = bs | (1 << 24);
	}

	ret = write_retry(info->outfd, info->block, bs);
	if (ret < 0) {
		perror("writing to output file");
		return -1;
	}

	if ((size_t)ret < bs) {
		fputs("write to output file truncated\n", stderr);
		return -1;
	}

	info->super.bytes_used += bs;
	return 0;
}

static int flush_fragments(sqfs_info_t *info)
{
	size_t newsz, size;
	file_info_t *fi;
	uint64_t offset;
	ssize_t ret;
	void *new;

	if (info->num_fragments == info->max_fragments) {
		newsz = info->max_fragments ? info->max_fragments * 2 : 16;
		new = realloc(info->fragments,
			      sizeof(info->fragments[0]) * newsz);

		if (new == NULL) {
			perror("appending to fragment table");
			return -1;
		}

		info->max_fragments = newsz;
		info->fragments = new;
	}

	offset = info->super.bytes_used;
	size = info->frag_offset;

	for (fi = info->frag_list; fi != NULL; fi = fi->frag_next)
		fi->fragment = info->num_fragments;

	ret = info->cmp->do_block(info->cmp, info->fragment, size);
	if (ret < 0)
		return -1;

	info->fragments[info->num_fragments].start_offset = htole64(offset);
	info->fragments[info->num_fragments].pad0 = 0;

	if (ret > 0) {
		size = ret;
		info->fragments[info->num_fragments].size = htole32(size);
	} else {
		info->fragments[info->num_fragments].size =
			htole32(size | (1 << 24));
	}

	info->num_fragments += 1;

	ret = write_retry(info->outfd, info->fragment, size);
	if (ret < 0) {
		perror("writing to output file");
		return -1;
	}

	if ((size_t)ret < size) {
		fputs("write to output file truncated\n", stderr);
		return -1;
	}

	memset(info->fragment, 0, info->super.block_size);

	info->super.bytes_used += size;
	info->frag_offset = 0;
	info->frag_list = NULL;

	info->super.flags &= ~SQFS_FLAG_NO_FRAGMENTS;
	info->super.flags |= SQFS_FLAG_ALWAYS_FRAGMENTS;
	return 0;
}

static int add_fragment(file_info_t *fi, sqfs_info_t *info, size_t size)
{
	if (info->frag_offset + size > info->super.block_size) {
		if (flush_fragments(info))
			return -1;
	}

	fi->fragment_offset = info->frag_offset;
	fi->frag_next = info->frag_list;
	info->frag_list = fi;

	memcpy((char *)info->fragment + info->frag_offset, info->block, size);
	info->frag_offset += size;
	return 0;
}

static int process_file(sqfs_info_t *info, file_info_t *fi)
{
	uint64_t count = fi->size;
	int infd, ret;
	size_t diff;

	infd = open(fi->input_file, O_RDONLY);
	if (infd < 0) {
		perror(fi->input_file);
		return -1;
	}

	fi->startblock = info->super.bytes_used;
	info->file_block_count = 0;

	while (count != 0) {
		diff = count > (uint64_t)info->super.block_size ?
			info->super.block_size : count;

		ret = read_retry(infd, info->block, diff);
		if (ret < 0)
			goto fail_read;
		if ((size_t)ret < diff)
			goto fail_trunc;

		if (diff < info->super.block_size) {
			if (add_fragment(fi, info, diff))
				goto fail;
		} else {
			if (write_block(fi, info))
				goto fail;
		}

		count -= diff;
	}

	close(infd);
	return 0;
fail:
	close(infd);
	return -1;
fail_read:
	fprintf(stderr, "read from %s: %s\n", fi->input_file, strerror(errno));
	goto fail;
fail_trunc:
	fprintf(stderr, "%s: truncated read\n", fi->input_file);
	goto fail;
}

static int find_and_process_files(sqfs_info_t *info, tree_node_t *n)
{
	if (S_ISDIR(n->mode)) {
		for (n = n->data.dir->children; n != NULL; n = n->next) {
			if (find_and_process_files(info, n))
				return -1;
		}
		return 0;
	}

	if (S_ISREG(n->mode))
		return process_file(info, n->data.file);

	return 0;
}

int write_data_to_image(sqfs_info_t *info)
{
	int ret;

	info->block = malloc(info->super.block_size);

	if (info->block == NULL) {
		perror("allocating data block buffer");
		return -1;
	}

	info->fragment = malloc(info->super.block_size);

	if (info->fragment == NULL) {
		perror("allocating fragment buffer");
		free(info->block);
		return -1;
	}

	ret = find_and_process_files(info, info->fs.root);

	free(info->block);
	free(info->fragment);

	info->block = NULL;
	info->fragment = NULL;
	return ret;
}
