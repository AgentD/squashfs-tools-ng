/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef UTIL_H
#define UTIL_H

#include <sys/types.h>

int canonicalize_name(char *filename);

ssize_t write_retry(int fd, void *data, size_t size);

ssize_t read_retry(int fd, void *buffer, size_t size);

void print_version(void);

int mkdir_p(const char *path);

#endif /* UTIL_H */
