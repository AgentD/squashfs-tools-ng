/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * compat.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef COMPAT_H
#define COMPAT_H

#include "sqfs/predef.h"
#include "config.h"

#include <limits.h>
#include <stdio.h>

/****************************** safe arithmetic ******************************/

#if defined(__GNUC__) && __GNUC__ >= 5
#	define SZ_ADD_OV __builtin_add_overflow
#	define SZ_MUL_OV __builtin_mul_overflow
#elif defined(__clang__) && defined(__GNUC__) && __GNUC__ < 5
#	if SIZE_MAX <= UINT_MAX
#		define SZ_ADD_OV __builtin_uadd_overflow
#		define SZ_MUL_OV __builtin_umul_overflow
#	elif SIZE_MAX == ULONG_MAX
#		define SZ_ADD_OV __builtin_uaddl_overflow
#		define SZ_MUL_OV __builtin_umull_overflow
#	elif SIZE_MAX == ULLONG_MAX
#		define SZ_ADD_OV __builtin_uaddll_overflow
#		define SZ_MUL_OV __builtin_umulll_overflow
#	else
#		error Cannot determine maximum value of size_t
#	endif
#else
static inline int _sz_add_overflow(size_t a, size_t b, size_t *res)
{
	*res = a + b;
	return (*res < a) ? 1 : 0;
}

static inline int _sz_mul_overflow(size_t a, size_t b, size_t *res)
{
	*res = a * b;
	return (b > 0 && (a > SIZE_MAX / b)) ? 1 : 0;
}
#	define SZ_ADD_OV _sz_add_overflow
#	define SZ_MUL_OV _sz_mul_overflow
#endif

/********************* printf macros & format specifiers *********************/

#if defined(__GNUC__) || defined(__clang__)
#	define PRINTF_ATTRIB(fmt, elipsis)			\
		__attribute__ ((format (printf, fmt, elipsis)))
#else
#	define PRINTF_ATTRIB(fmt, elipsis)
#endif

#if defined(_WIN32) || defined(__WINDOWS__)
#	define PRI_U64 "%I64u"
#	define PRI_U32 "%I32u"
#else
#	include <inttypes.h>
#	define PRI_U64 "%" PRIu64
#	define PRI_U32 "%" PRIu32
#endif

#if SIZE_MAX <= UINT_MAX
#	define PRI_SZ "%u"
#elif SIZE_MAX == ULONG_MAX
#	define PRI_SZ "%lu"
#elif defined(_WIN32) && SIZE_MAX == UINT64_MAX
#	define PRI_SZ "%I64u"
#else
#	error Cannot figure out propper printf specifier for size_t
#endif

/***************************** endian conversion *****************************/

#if defined(__APPLE__)
#include <libkern/OSByteOrder.h>

#define htole16(x) OSSwapHostToLittleInt16(x)
#define htole32(x) OSSwapHostToLittleInt32(x)
#define htole64(x) OSSwapHostToLittleInt64(x)

#define le32toh(x) OSSwapLittleToHostInt32(x)
#define le16toh(x) OSSwapLittleToHostInt16(x)
#define le64toh(x) OSSwapLittleToHostInt64(x)
#elif defined(__DragonFly__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/endian.h>
#elif defined(_WIN32) || defined(__WINDOWS__)
#define htole16(x) (x)
#define htole32(x) (x)
#define htole64(x) (x)

#define le16toh(x) (x)
#define le32toh(x) (x)
#define le64toh(x) (x)
#else
#include <endian.h>
#endif

/**************************** struct stat related ****************************/

#if defined(_WIN32) || defined(__WINDOWS__)
#include "sqfs/inode.h"

#define S_IFSOCK SQFS_INODE_MODE_SOCK
#define S_IFLNK SQFS_INODE_MODE_LNK
#define S_IFREG SQFS_INODE_MODE_REG
#define S_IFBLK SQFS_INODE_MODE_BLK
#define S_IFDIR SQFS_INODE_MODE_DIR
#define S_IFCHR SQFS_INODE_MODE_CHR
#define S_IFIFO SQFS_INODE_MODE_FIFO
#define S_IFMT SQFS_INODE_MODE_MASK

#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#define S_ISCHR(m) (((m) & S_IFMT) == S_IFCHR)
#define S_ISBLK(m) (((m) & S_IFMT) == S_IFBLK)
#define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)
#define S_ISLNK(m) (((m) & S_IFMT) == S_IFLNK)
#define S_ISSOCK(m) (((m) & S_IFMT) == S_IFSOCK)

#define S_ISUID SQFS_INODE_SET_UID
#define S_ISGID SQFS_INODE_SET_GID
#define S_ISVTX SQFS_INODE_STICKY

#define S_IRWXU SQFS_INODE_OWNER_MASK
#define S_IRUSR SQFS_INODE_OWNER_R
#define S_IWUSR SQFS_INODE_OWNER_W
#define S_IXUSR SQFS_INODE_OWNER_X

#define S_IRWXG SQFS_INODE_GROUP_MASK
#define S_IRGRP SQFS_INODE_GROUP_R
#define S_IWGRP SQFS_INODE_GROUP_W
#define S_IXGRP SQFS_INODE_GROUP_X

#define S_IRWXO SQFS_INODE_OTHERS_MASK
#define S_IROTH SQFS_INODE_OTHERS_R
#define S_IWOTH SQFS_INODE_OTHERS_W
#define S_IXOTH SQFS_INODE_OTHERS_X

/* lifted from musl libc */
#define major(x) \
	((unsigned)( (((x)>>31>>1) & 0xfffff000) | (((x)>>8) & 0x00000fff) ))

#define minor(x)							\
	((unsigned)( (((x)>>12) & 0xffffff00) | ((x) & 0x000000ff) ))

#define makedev(x,y) ( \
        (((x)&0xfffff000ULL) << 32) | \
	(((x)&0x00000fffULL) << 8) | \
        (((y)&0xffffff00ULL) << 12) | \
	(((y)&0x000000ffULL)) )
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#if defined(__linux__) || defined(__GLIBC__)
#include <sys/sysmacros.h>
#endif
#endif

/***************************** missing functions *****************************/

#ifndef HAVE_STRNDUP
char *strndup(const char *str, size_t max_len);
#endif

#ifndef HAVE_GETOPT
extern char *optarg;
extern int optind, opterr, optopt, optpos, optreset;

void __getopt_msg(const char *a, const char *b, const char *c, size_t l);

int getopt(int argc, char * const argv[], const char *optstring);
#endif

#ifndef HAVE_GETOPT_LONG
struct option {
	const char *name;
	int has_arg;
	int *flag;
	int val;
};

#define no_argument        0
#define required_argument  1
#define optional_argument  2

int getopt_long(int, char *const *, const char *,
		const struct option *, int *);
#endif

#ifndef HAVE_GETSUBOPT
int getsubopt(char **opt, char *const *keys, char **val);
#endif

#ifdef HAVE_FNMATCH
#include <fnmatch.h>
#else
#define	FNM_PATHNAME 0x1

#define	FNM_NOMATCH 1
#define FNM_NOSYS   (-1)

int fnmatch(const char *, const char *, int);
#endif

#ifndef HAVE_STRCHRNUL
char *strchrnul(const char *s, int c);
#endif

/************************** Windows related madness **************************/

#if defined(_WIN32) || defined(__WINDOWS__)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define AT_FDCWD ((int)0xDEADBEEF)
#define AT_SYMLINK_NOFOLLOW (0x01)

typedef SSIZE_T ssize_t;

int fchownat(int dirfd, const char *path, int uid, int gid, int flags);

int fchmodat(int dirfd, const char *path, int mode, int flags);

int chdir(const char *path);

void w32_perror(const char *str);

WCHAR *path_to_windows(const char *input);

extern int sqfs_tools_main(int argc, char **argv);

int sqfs_tools_fputc(int c, FILE *strm);
int sqfs_tools_fputs(const char *str, FILE *strm);
int sqfs_tools_printf(const char *fmt, ...) PRINTF_ATTRIB(1, 2);
int sqfs_tools_fprintf(FILE *strm, const char *fmt, ...) PRINTF_ATTRIB(2, 3);

#define main sqfs_tools_main
#define printf sqfs_tools_printf
#define fprintf sqfs_tools_fprintf
#define fputs sqfs_tools_fputs
#define fputc sqfs_tools_fputc
#define putc sqfs_tools_fputc
#endif

/****************************** error handling ******************************/

#if defined(_WIN32) || defined(__WINDOWS__)
typedef struct {
	int unix_errno;
	DWORD w32_errno;
} os_error_t;

static SQFS_INLINE os_error_t get_os_error_state(void)
{
	os_error_t out = { errno, GetLastError() };
	return out;
}

static SQFS_INLINE void set_os_error_state(os_error_t err)
{
	SetLastError(err.w32_errno);
	errno = err.unix_errno;
}
#else
#include <errno.h>

typedef int os_error_t;

#define get_os_error_state() errno
#define set_os_error_state(err) errno = (err)
#endif

#endif /* COMPAT_H */
