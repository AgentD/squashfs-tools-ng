/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * tar2sqfs.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "tar2sqfs.h"

static int tar_probe(const sqfs_u8 *data, size_t size)
{
	size_t i, offset;

	if (size >= TAR_RECORD_SIZE) {
		for (i = 0; i < TAR_RECORD_SIZE; ++i) {
			if (data[i] != 0x00)
				break;
		}

		if (i == TAR_RECORD_SIZE) {
			data += TAR_RECORD_SIZE;
			size -= TAR_RECORD_SIZE;
		}
	}

	offset = offsetof(tar_header_t, magic);

	if (offset + 5 <= size) {
		if (memcmp(data + offset, "ustar", 5) == 0)
			return 1;
	}

	return 0;
}

int main(int argc, char **argv)
{
	int status = EXIT_FAILURE;
	istream_t *input_file = NULL;
	sqfs_writer_t sqfs;
	int ret;

	process_args(argc, argv);

	input_file = istream_open_stdin();
	if (input_file == NULL)
		return EXIT_FAILURE;

	ret = istream_detect_compressor(input_file, tar_probe);
	if (ret < 0)
		goto out_if;

	if (ret > 0) {
		if (!fstream_compressor_exists(ret)) {
			fprintf(stderr,
				"%s: %s compression is not supported.\n",
				istream_get_filename(input_file),
				fstream_compressor_name_from_id(ret));
			goto out_if;
		}

		input_file = istream_compressor_create(input_file, ret);
		if (input_file == NULL)
			return EXIT_FAILURE;
	}

	memset(&sqfs, 0, sizeof(sqfs));
	if (sqfs_writer_init(&sqfs, &cfg))
		goto out_if;

	if (process_tarball(input_file, &sqfs))
		goto out;

	if (fstree_post_process(&sqfs.fs))
		goto out;

	if (sqfs_writer_finish(&sqfs, &cfg))
		goto out;

	status = EXIT_SUCCESS;
out:
	sqfs_writer_cleanup(&sqfs, status);
out_if:
	sqfs_destroy(input_file);
	return status;
}
