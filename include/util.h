/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * util.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef UTIL_H
#define UTIL_H

#include "config.h"

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


/* layout structure for sparse files, indicating where the actual data is */
typedef struct sparse_map_t {
	struct sparse_map_t *next;
	uint64_t offset;
	uint64_t count;
} sparse_map_t;

/*
  Convert back to forward slashed, remove all preceeding and trailing slashes,
  collapse all sequences of slashes, remove all path components that are '.'
  and returns failure state if one of the path components is '..'.

  Returns 0 on success.
*/
int canonicalize_name(char *filename);

/*
  A wrapper around the write() system call. It retries the write if it is
  interrupted by a signal or only part of the data was written. Returns 0
  on success. Writes to stderr on failure using 'errstr' as a perror style
  error prefix.
*/
int write_data(const char *errstr, int fd, const void *data, size_t size);

/*
  A wrapper around the read() system call. It retries the read if it is
  interrupted by a signal or less than the desired size was read. Returns 0
  on success. Writes to stderr on failure using 'errstr' as a perror style
  error prefix.
*/
int read_data(const char *errstr, int fd, void *buffer, size_t size);

/*
  Similar to read_data but wrapps pread() instead of read().
*/
int read_data_at(const char *errstr, off_t location,
		 int fd, void *buffer, size_t size);

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

/*
  Write zero bytes to an output file to padd it to specified block size.
  Returns 0 on success. On failure, prints error message to stderr.
*/
int padd_file(int outfd, uint64_t size, size_t blocksize);

/*
  If the environment variable SOURCE_DATE_EPOCH is set to a parsable number
  that fits into an unsigned 32 bit value, return its value. Otherwise,
  default to 0.
 */
uint32_t get_source_date_epoch(void);

#endif /* UTIL_H */
