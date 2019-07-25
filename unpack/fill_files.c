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

static file_info_t *split_list(file_info_t *list, uint64_t threashold)
{
	file_info_t *it, *new = NULL;
	uint64_t size = 0;

	for (it = list; it != NULL; it = it->next) {
		if (it->input_file == NULL)
			continue;

		size += it->size - it->sparse;

		if (size >= threashold) {
			new = it->next;
			it->next = NULL;
			break;
		}
	}

	return new;
}

static uint64_t total_size(file_info_t *list)
{
	uint64_t size = 0;
	file_info_t *it;

	for (it = list; it != NULL; it = it->next) {
		if (it->input_file != NULL)
			size += it->size - it->sparse;
	}

	return size;
}

int fill_unpacked_files(fstree_t *fs, data_reader_t *data, int flags,
			unsigned int num_jobs)
{
	file_info_t *sublists[num_jobs], *it;
	int exitstatus, status = 0;
	uint64_t threshold;
	unsigned int i;
	pid_t pid;

	if (num_jobs <= 1) {
		status = fill_files(data, fs->files, flags);

		for (it = fs->files; it != NULL; it = it->next)
			free(it->input_file);

		return status;
	}

	threshold = total_size(fs->files) / num_jobs;

	for (i = 0; i < num_jobs; ++i) {
		sublists[i] = fs->files;

		fs->files = split_list(fs->files, threshold);
	}

	for (i = 0; i < num_jobs; ++i) {
		pid = fork();

		if (pid == 0) {
			if (fill_files(data, sublists[i], flags))
				exit(EXIT_FAILURE);
			exit(EXIT_SUCCESS);
		}

		if (pid < 0) {
			perror("fork");
			status = -1;
			num_jobs = i;
			break;
		}
	}

	for (i = 0; i < num_jobs; ++i) {
		do {
			pid = waitpid(-1, &exitstatus, 0);
			if (errno == ECHILD)
				goto out;
		} while (pid < 0);

		if (!WIFEXITED(exitstatus) ||
		    WEXITSTATUS(exitstatus) != EXIT_SUCCESS) {
			status = -1;
		}

		for (it = sublists[i]; it != NULL; it = it->next)
			free(it->input_file);
	}
out:
	return status;
}
