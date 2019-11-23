/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * util.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef COMPAT_H
#define COMPAT_H

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

struct stat {
	sqfs_u32 st_dev;
	sqfs_u32 st_ino;
	sqfs_u16 st_mode;
	sqfs_u16 st_nlink;
	sqfs_u32 st_uid;
	sqfs_u32 st_gid;
	sqfs_u32 st_rdev;
	sqfs_u64 st_size;
	sqfs_u32 st_blksize;
	sqfs_u32 st_blocks;
	sqfs_u64 st_atime;
	sqfs_u64 st_mtime;
	sqfs_u64 st_ctime;
};

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

#define AT_FDCWD (0xDEADBEEF)
#define AT_SYMLINK_NOFOLLOW (0x01)

int fchownat(int dirfd, const char *path, int uid, int gid, int flags);

int fchmodat(int dirfd, const char *path, int mode, int flags);
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#ifdef __linux__
#include <sys/sysmacros.h>
#endif
#endif

#ifndef HAVE_GETLINE
#include <stdio.h>

ssize_t getline(char **line, size_t *n, FILE *fp);
#endif

#ifndef HAVE_STRNDUP
char *strndup(const char *str, size_t max_len);
#endif

#ifndef HAVE_GETSUBOPT
int getsubopt(char **opt, char *const *keys, char **val);
#endif

#endif /* COMPAT_H */
