/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef UTIL_H
#define UTIL_H

#include <sys/types.h>

/*
  Removes all preceeding and trailing slashes, shortens all sequences of
  slashes to a single slash and returns failure state if one of the path
  components is '..' or '.'.

  Returns 0 on success.
*/
int canonicalize_name(char *filename);

/*
  A wrapper around the write() system call. It retries the write if it is
  interrupted by a signal or only part of the data was written.
*/
ssize_t write_retry(int fd, const void *data, size_t size);

/*
  A wrapper around the read() system call. It retries the read if it is
  interrupted by a signal or less than the desired size was read.
*/
ssize_t read_retry(int fd, void *buffer, size_t size);

/*
  A common implementation of the '--version' command line flag.

  Prints out version information. The program name is extracted from the
  BSD style __progname global variable.
*/
void print_version(void);

/*
  A wrapper around mkdir() that behaves like 'mkdir -p'. It tries to create
  every component of the given path and skips already existing entries.

  Returns 0 on success.
*/
int mkdir_p(const char *path);

/* Returns 0 on success. On failure, prints error message to stderr. */
int pushd(const char *path);

/* Same as pushd, but the string doesn't have to be null-terminated. */
int pushdn(const char *path, size_t len);

/* Returns 0 on success. On failure, prints error message to stderr. */
int popd(void);

#endif /* UTIL_H */
