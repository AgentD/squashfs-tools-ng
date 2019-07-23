/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include "rdsquashfs.h"

static int fill_files(data_reader_t *data, file_info_t *list, int flags)
{
	file_info_t *fi;
	int fd;

	for (fi = list; fi != NULL; fi = fi->next) {
		if (fi->input_file == NULL)
			continue;

		fd = open(fi->input_file, O_WRONLY);
		if (fd < 0) {
			fprintf(stderr, "unpacking %s: %s\n",
				fi->input_file, strerror(errno));
			return -1;
		}

		if (!(flags & UNPACK_QUIET))
			printf("unpacking %s\n", fi->input_file);

		if (data_reader_dump_file(data, fi, fd,
					  (flags & UNPACK_NO_SPARSE) == 0)) {
			close(fd);
			return -1;
		}

		close(fd);
	}

	return 0;
}

int fill_unpacked_files(fstree_t *fs, data_reader_t *data, int flags)
{
	return fill_files(data, fs->files, flags);
}
