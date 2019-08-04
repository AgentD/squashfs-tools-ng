/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * fill_files.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
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

int fill_unpacked_files(fstree_t *fs, data_reader_t *data, int flags,
			unsigned int num_jobs)
{
	file_info_t **sublists, *it;
	int exitstatus, status = 0;
	unsigned int i;
	pid_t pid;

	if (num_jobs < 1)
		num_jobs = 1;

	sublists = alloca(sizeof(sublists[0]) * num_jobs);
	optimize_unpack_order(fs, num_jobs, sublists);

	if (num_jobs < 2) {
		status = fill_files(data, sublists[0], flags);
		goto out;
	}

	for (i = 0; i < num_jobs; ++i) {
		pid = fork();

		if (pid == 0) {
			/* Kill the child when the parent process dies */
			prctl(PR_SET_PDEATHSIG, SIGKILL);

			if (fill_files(data, sublists[i], flags))
				exit(EXIT_FAILURE);
			exit(EXIT_SUCCESS);
		}

		if (pid < 0) {
			perror("fork");
			status = -1;
			break;
		}
	}

	for (;;) {
		errno = 0;
		pid = waitpid(-1, &exitstatus, 0);

		if (pid < 0) {
			if (errno == EINTR)
				continue;
			if (errno == ECHILD)
				break;
		}

		if (!WIFEXITED(exitstatus) ||
		    WEXITSTATUS(exitstatus) != EXIT_SUCCESS) {
			status = -1;
		}
	}
out:
	for (i = 0; i < num_jobs; ++i) {
		for (it = sublists[i]; it != NULL; it = it->next)
			free(it->input_file);
	}

	return status;
}
