/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * optimize_unpack_order.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "rdsquashfs.h"

static bool has_fragment(const fstree_t *fs, const file_info_t *file)
{
	if (file->size % fs->block_size == 0)
		return false;

	return file->fragment_offset < fs->block_size &&
		(file->fragment != 0xFFFFFFFF);
}

static int compare_files(const fstree_t *fs, const file_info_t *lhs,
			 const file_info_t *rhs)
{
	/* NOOP < everything else */
	if (lhs->input_file == NULL)
		return rhs->input_file == NULL ? 0 : -1;

	if (rhs->input_file == NULL)
		return 1;

	/* Files with fragments come first, ordered by ID.
	   In case of tie, files without data blocks come first,
	   and the others are ordered by start block. */
	if (has_fragment(fs, lhs)) {
		if (!(has_fragment(fs, rhs)))
			return -1;

		if (lhs->fragment < rhs->fragment)
			return -1;
		if (lhs->fragment > rhs->fragment)
			return 1;

		if (lhs->size < fs->block_size)
			return (rhs->size < fs->block_size) ? 0 : -1;
		if (rhs->size < fs->block_size)
			return 1;
		goto order_by_start;
	}

	if (has_fragment(fs, rhs))
		return 1;

	/* order the rest by start block */
order_by_start:
	return lhs->startblock < rhs->startblock ? -1 :
		lhs->startblock > rhs->startblock ? 1 : 0;
}

/* TODO: unify ad-hoc merge sort with the one in fstree_sort */
static file_info_t *merge(const fstree_t *fs, file_info_t *lhs,
			  file_info_t *rhs)
{
	file_info_t *it;
	file_info_t *head = NULL;
	file_info_t **next_ptr = &head;

	while (lhs != NULL && rhs != NULL) {
		if (compare_files(fs, lhs, rhs) <= 0) {
			it = lhs;
			lhs = lhs->next;
		} else {
			it = rhs;
			rhs = rhs->next;
		}

		*next_ptr = it;
		next_ptr = &it->next;
	}

	it = (lhs != NULL ? lhs : rhs);
	*next_ptr = it;
	return head;
}

static file_info_t *list_sort(const fstree_t *fs, file_info_t *head)
{
	file_info_t *it, *half, *prev;

	it = half = prev = head;
	while (it != NULL) {
		prev = half;
		half = half->next;
		it = it->next;
		if (it != NULL) {
			it = it->next;
		}
	}

	// half refers to the (count/2)'th element ROUNDED UP.
	// It will be null therefore only in the empty and the
	// single element list
	if (half == NULL) {
		return head;
	}

	prev->next = NULL;

	return merge(fs, list_sort(fs, head), list_sort(fs, half));
}

file_info_t *optimize_unpack_order(fstree_t *fs)
{
	file_info_t *file_list;

	file_list = list_sort(fs, fs->files);
	while (file_list != NULL && file_list->input_file == NULL)
		file_list = file_list->next;

	fs->files = NULL;
	return file_list;
}
