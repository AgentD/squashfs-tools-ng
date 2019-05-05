/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef UTIL_H
#define UTIL_H

#include <sys/types.h>

/**
 * @brief Turn a file path to a more usefull form
 *
 * Removes all preceeding and trailing slashes, shortens all sequences of
 * slashes to a single slash and returns failure state if one of the path
 * components is '..' or '.'.
 *
 * @param filename A pointer to the path to work on
 *
 * @return Zero on success, -1 on failure
 */
int canonicalize_name(char *filename);

/**
 * @brief Write data to a file
 *
 * This is a wrapper around the Unix write() system call. It retries the write
 * if it is interrupted by a signal or only part of the data was written.
 */
ssize_t write_retry(int fd, void *data, size_t size);

/**
 * @brief Read data from a file
 *
 * This is a wrapper around the Unix read() system call. It retries the read
 * if it is interrupted by a signal or less than the desired size was read.
 */
ssize_t read_retry(int fd, void *buffer, size_t size);

/**
 * @brief A common implementation of the '--version' command line argument
 *
 * Prints out version information. The program name is extracted from the
 * BSD style __progname global variable.
 */
void print_version(void);

/**
 * @brief Create a directory and all its parents
 *
 * This is a wrapper around mkdir() that behaves like 'mkdir -p'. It tries to
 * create every component of the given path and treats already existing entries
 * as success.
 *
 * @param path A path to create
 *
 * @return Zero on success, -1 on failure
 */
int mkdir_p(const char *path);

#endif /* UTIL_H */
