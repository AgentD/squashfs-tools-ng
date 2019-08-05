/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * compare_file.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "difftool.h"

int compare_files(file_info_t *a, file_info_t *b, const char *path)
{
	uint64_t offset, size, diff;
	int ret = 0, afd, bfd;
	void *aptr, *bptr;

	if (a->size != b->size)
		return 1;

	if (pushd(first_path))
		return -1;
	afd = open(path, O_RDONLY);
	if (afd < 0) {
		fprintf(stderr, "%s/%s: %s\n", first_path, path,
			strerror(errno));
		return -1;
	}
	if (popd())
		goto fail_afd;

	if (pushd(second_path))
		goto fail_afd;
	bfd = open(path, O_RDONLY);
	if (bfd < 0) {
		fprintf(stderr, "%s/%s: %s\n", second_path, path,
			strerror(errno));
		goto fail_afd;
	}
	if (popd())
		goto fail_bfd;

	size = a->size;

	for (offset = 0; offset < size; offset += diff) {
		diff = size - offset;
		if (diff > MAX_WINDOW_SIZE)
			diff = MAX_WINDOW_SIZE;

		aptr = mmap(NULL, diff, PROT_READ, MAP_SHARED, afd, offset);
		if (aptr == MAP_FAILED) {
			fprintf(stderr, "mmap %s/%s: %s\n", first_path, path,
				strerror(errno));
			goto fail_bfd;
		}

		bptr = mmap(NULL, diff, PROT_READ, MAP_SHARED, bfd, offset);
		if (bptr == MAP_FAILED) {
			fprintf(stderr, "mmap %s/%s: %s\n", second_path, path,
				strerror(errno));
			goto fail_bfd;
		}

		ret = memcmp(aptr, bptr, diff);
		munmap(aptr, diff);
		munmap(bptr, diff);

		if (ret != 0)
			break;
	}

	close(afd);
	close(bfd);
	return ret;
fail_bfd:
	close(bfd);
fail_afd:
	close(afd);
	return -1;
}
