/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "mkfs.h"
#include "util.h"

static int write_compressed(sqfs_info_t *info, const void *in, size_t size,
			    uint32_t *outsize)
{
	ssize_t ret;

	ret = info->cmp->do_block(info->cmp, in, size, info->scratch,
				  info->super.block_size);
	if (ret < 0)
		return -1;

	if (ret > 0 && (size_t)ret < size) {
		size = ret;
		ret = write_retry(info->outfd, info->scratch, size);
		*outsize = size;
	} else {
		ret = write_retry(info->outfd, in, size);
		*outsize = size | (1 << 24);
	}

	if (ret < 0) {
		perror("writing to output file");
		return -1;
	}

	if ((size_t)ret < size) {
		fputs("write to output file truncated\n", stderr);
		return -1;
	}

	info->super.bytes_used += ret;
	return 0;
}

static int grow_fragment_table(sqfs_info_t *info)
{
	size_t newsz;
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

	return 0;
}

static int flush_fragments(sqfs_info_t *info)
{
	uint64_t offset;
	uint32_t out;

	if (grow_fragment_table(info))
		return -1;

	offset = info->super.bytes_used;

	if (write_compressed(info, info->fragment, info->frag_offset, &out))
		return -1;

	info->fragments[info->num_fragments].start_offset = htole64(offset);
	info->fragments[info->num_fragments].pad0 = 0;
	info->fragments[info->num_fragments].size = htole32(out);

	info->num_fragments += 1;
	info->frag_offset = 0;

	info->super.flags &= ~SQFS_FLAG_NO_FRAGMENTS;
	info->super.flags |= SQFS_FLAG_ALWAYS_FRAGMENTS;
	return 0;
}

static int write_data_from_fd(sqfs_info_t *info, file_info_t *fi, int infd)
{
	uint64_t count = fi->size;
	int blk_idx = 0;
	uint32_t out;
	ssize_t ret;
	size_t diff;

	fi->startblock = info->super.bytes_used;

	while (count != 0) {
		diff = count > (uint64_t)info->super.block_size ?
			info->super.block_size : count;

		ret = read_retry(infd, info->block, diff);
		if (ret < 0)
			goto fail_read;
		if ((size_t)ret < diff)
			goto fail_trunc;

		if (diff < info->super.block_size) {
			if (info->frag_offset + diff > info->super.block_size) {
				if (flush_fragments(info))
					return -1;
			}

			fi->fragment_offset = info->frag_offset;
			fi->fragment = info->num_fragments;

			memcpy((char *)info->fragment + info->frag_offset,
			       info->block, diff);
			info->frag_offset += diff;
		} else {
			if (write_compressed(info, info->block,
					     info->super.block_size, &out)) {
				return -1;
			}

			fi->blocksizes[blk_idx++] = out;
		}

		count -= diff;
	}

	return 0;
fail_read:
	fprintf(stderr, "read from %s: %s\n", fi->input_file, strerror(errno));
	return -1;
fail_trunc:
	fprintf(stderr, "%s: truncated read\n", fi->input_file);
	return -1;
}

static int process_file(sqfs_info_t *info, file_info_t *fi)
{
	int ret, infd;

	infd = open(fi->input_file, O_RDONLY);
	if (infd < 0) {
		perror(fi->input_file);
		return -1;
	}

	ret = write_data_from_fd(info, fi, infd);

	close(infd);
	return ret;
}

static void print_name(tree_node_t *n)
{
	if (n->parent != NULL) {
		print_name(n->parent);
		fputc('/', stdout);
	}

	fputs(n->name, stdout);
}

static int find_and_process_files(sqfs_info_t *info, tree_node_t *n,
				  bool quiet)
{
	if (S_ISDIR(n->mode)) {
		for (n = n->data.dir->children; n != NULL; n = n->next) {
			if (find_and_process_files(info, n, quiet))
				return -1;
		}
		return 0;
	}

	if (S_ISREG(n->mode)) {
		if (!quiet) {
			fputs("packing ", stdout);
			print_name(n);
			fputc('\n', stdout);
		}

		return process_file(info, n->data.file);
	}

	return 0;
}

int write_data_to_image(sqfs_info_t *info)
{
	bool need_restore = false;
	const char *ptr;
	int ret;

	if (info->opt.packdir != NULL) {
		if (pushd(info->opt.packdir))
			return -1;
		need_restore = true;
	} else {
		ptr = strrchr(info->opt.infile, '/');

		if (ptr != NULL) {
			if (pushdn(info->opt.infile, ptr - info->opt.infile))
				return -1;

			need_restore = true;
		}
	}

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

	info->scratch = malloc(info->super.block_size);
	if (info->scratch == NULL) {
		perror("allocating scratch buffer");
		free(info->block);
		free(info->fragment);
		return -1;
	}

	ret = find_and_process_files(info, info->fs.root, info->opt.quiet);

	ret = ret == 0 ? flush_fragments(info) : ret;

	free(info->block);
	free(info->fragment);
	free(info->scratch);

	info->block = NULL;
	info->fragment = NULL;
	info->scratch = NULL;

	if (need_restore)
		ret = popd();

	return ret;
}
