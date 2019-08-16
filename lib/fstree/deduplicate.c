/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * deduplicate.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "fstree.h"

#include <string.h>

file_info_t *fragment_by_chksum(file_info_t *fi, uint32_t chksum,
				size_t frag_size, file_info_t *list,
				size_t block_size)
{
	file_info_t *it;

	for (it = list; it != NULL; it = it->next) {
		if (it == fi)
			continue;

		if (!(it->flags & FILE_FLAG_HAS_FRAGMENT))
			continue;

		if (it->flags & FILE_FLAG_FRAGMENT_IS_DUPLICATE)
			continue;

		if ((it->size % block_size) != frag_size)
			continue;

		if (it->fragment_chksum == chksum)
			break;
	}

	return it;
}

static size_t get_block_count(file_info_t *fi, size_t block_size)
{
	size_t count = fi->size / block_size;

	if ((fi->size % block_size) && !(fi->flags & FILE_FLAG_HAS_FRAGMENT))
		++count;

	while (count > 0 && fi->blocks[count - 1].size == 0)
		--count;

	return count;
}

static size_t find_first_match(file_info_t *file, file_info_t *cmp,
			       size_t idx, size_t cmp_blk_count)
{
	size_t i;

	for (i = 0; i < cmp_blk_count; ++i) {
		if (memcmp(file->blocks + idx, cmp->blocks + i,
			   sizeof(file->blocks[idx])) == 0) {
			break;
		}
	}

	return i;
}

uint64_t find_equal_blocks(file_info_t *file, file_info_t *list,
			   size_t block_size)
{
	size_t start, first_match, i, j, block_count, cmp_blk_count;
	uint64_t location;
	file_info_t *it;

	block_count = get_block_count(file, block_size);
	if (block_count == 0)
		return 0;

	for (start = 0; start < block_count; ++start) {
		if (file->blocks[start].size != 0)
			break;
	}

	for (it = list; it != NULL; it = it->next) {
		if (it == file)
			continue;

		if (it->flags & FILE_FLAG_BLOCKS_ARE_DUPLICATE)
			continue;

		cmp_blk_count = get_block_count(it, block_size);
		if (cmp_blk_count == 0)
			continue;

		first_match = find_first_match(file, it, start, cmp_blk_count);
		if (first_match == cmp_blk_count)
			continue;

		i = start;
		j = first_match;

		while (i < block_count && j < cmp_blk_count) {
			if (file->blocks[i].size == 0) {
				++i;
				continue;
			}

			if (it->blocks[j].size == 0) {
				++j;
				continue;
			}

			if (memcmp(it->blocks + j, file->blocks + i,
				   sizeof(file->blocks[i])) != 0) {
				break;
			}

			++i;
			++j;
		}

		if (i == block_count)
			break;
	}

	if (it == NULL)
		return 0;

	location = it->startblock;

	for (i = 0; i < first_match; ++i)
		location += it->blocks[i].size & ((1 << 24) - 1);

	return location;
}
