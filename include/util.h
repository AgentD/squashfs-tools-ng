/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * util.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef UTIL_H
#define UTIL_H

#include "config.h"
#include "sqfs/predef.h"

#include <sys/types.h>
#include <stdint.h>
#include <stddef.h>

#if defined(__GNUC__) || defined(__clang__)
#define UI_ADD_OV __builtin_uadd_overflow
#define UL_ADD_OV __builtin_uaddl_overflow
#define ULL_ADD_OV __builtin_uaddll_overflow

#define UI_MUL_OV __builtin_umul_overflow
#define UL_MUL_OV __builtin_umull_overflow
#define ULL_MUL_OV __builtin_umulll_overflow
#else
#error Sorry, I do not know how to trap integer overflows with this compiler
#endif

#if SIZEOF_SIZE_T <= SIZEOF_INT
#define SZ_ADD_OV UI_ADD_OV
#define SZ_MUL_OV UI_MUL_OV
#elif SIZEOF_SIZE_T == SIZEOF_LONG
#define SZ_ADD_OV UL_ADD_OV
#define SZ_MUL_OV UL_MUL_OV
#elif SIZEOF_SIZE_T == SIZEOF_LONG_LONG
#define SZ_ADD_OV ULL_ADD_OV
#define SZ_MUL_OV ULL_MUL_OV
#else
#error Cannot determine maximum value of size_t
#endif

/*
  A wrapper around the write() system call. It retries the write if it is
  interrupted by a signal or only part of the data was written. Returns 0
  on success. Writes to stderr on failure using 'errstr' as a perror style
  error prefix.
*/
SQFS_INTERNAL
int write_data(const char *errstr, int fd, const void *data, size_t size);

/*
  A wrapper around the read() system call. It retries the read if it is
  interrupted by a signal or less than the desired size was read. Returns 0
  on success. Writes to stderr on failure using 'errstr' as a perror style
  error prefix.
*/
SQFS_INTERNAL
int read_data(const char *errstr, int fd, void *buffer, size_t size);

/*
  Similar to read_data but wrapps pread() instead of read().
*/
SQFS_INTERNAL
int read_data_at(const char *errstr, off_t location,
		 int fd, void *buffer, size_t size);

/*
  A common implementation of the '--version' command line flag.

  Prints out version information. The program name is extracted from the
  BSD style __progname global variable.
*/
SQFS_INTERNAL
void print_version(void);

/*
  A wrapper around mkdir() that behaves like 'mkdir -p'. It tries to create
  every component of the given path and skips already existing entries.

  Returns 0 on success.
*/
SQFS_INTERNAL
int mkdir_p(const char *path);

/* Returns 0 on success. On failure, prints error message to stderr. */
SQFS_INTERNAL
int pushd(const char *path);

/* Same as pushd, but the string doesn't have to be null-terminated. */
SQFS_INTERNAL
int pushdn(const char *path, size_t len);

/* Returns 0 on success. On failure, prints error message to stderr. */
SQFS_INTERNAL
int popd(void);

/*
  Write zero bytes to an output file to padd it to specified block size.
  Returns 0 on success. On failure, prints error message to stderr.
*/
SQFS_INTERNAL
int padd_file(int outfd, uint64_t size, size_t blocksize);

SQFS_INTERNAL
int padd_sqfs(sqfs_file_t *file, uint64_t size, size_t blocksize);

/*
  Helper for allocating data structures with flexible array members.

  'base_size' is the size of the struct itself, 'item_size' the size of a
  single array element and 'nmemb' the number of elements.

  Iternally checks for arithmetic overflows when allocating the combined thing.
 */
SQFS_INTERNAL
void *alloc_flex(size_t base_size, size_t item_size, size_t nmemb);

/* Basically the same as calloc, but *ALWAYS* does overflow checking */
SQFS_INTERNAL
void *alloc_array(size_t item_size, size_t nmemb);

/* allocates len + 1 (for the null-terminator) and does overflow checking */
SQFS_INTERNAL
void *alloc_string(size_t len);

/*
  Convert back to forward slashed, remove all preceeding and trailing slashes,
  collapse all sequences of slashes, remove all path components that are '.'
  and returns failure state if one of the path components is '..'.

  Returns 0 on success.
*/
SQFS_INTERNAL int canonicalize_name(char *filename);

#endif /* UTIL_H */
