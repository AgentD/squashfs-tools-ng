/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * dir_iterator.h
 *
 * Copyright (C) 2023 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef IO_DIR_ITERATOR_H
#define IO_DIR_ITERATOR_H

#include "sqfs/predef.h"
#include "io/istream.h"
#include "io/xattr.h"

typedef enum {
	DIR_ENTRY_FLAG_MOUNT_POINT = 0x0001,

	DIR_ENTRY_FLAG_HARD_LINK = 0x0002,
} DIR_ENTRY_FLAG;

/**
 * @struct dir_entry_t
 *
 * @brief A directory entry returned by a @ref dir_iterator_t
 */
typedef struct {
	/**
	 * @brief Unix time stamp when the entry was last modified.
	 *
	 * If necessary, the OS native time stamp is converted to Unix time.
	 */
	sqfs_s64 mtime;

	/**
	 * @brief Device number where the entry is stored on.
	 *
	 * On Windows and other non-Unix OSes, a dummy value is stored here.
	 */
	sqfs_u64 dev;

	/**
	 * @brief Device number for device special files.
	 *
	 * On Windows and other non-Unix OSes, a dummy value is stored here.
	 */
	sqfs_u64 rdev;

	/**
	 * @brief ID of the user that owns the entry.
	 *
	 * On Windows and other non-Unix OSes, this always reports user 0.
	 */
	sqfs_u32 uid;

	/**
	 * @brief ID of the group that owns the entry.
	 *
	 * On Windows and other non-Unix OSes, this always reports group 0.
	 */
	sqfs_u32 gid;

	/**
	 * @brief Unix style permissions and entry type.
	 *
	 * On Windows and other non-Unix OSes, this is synthesized from the
	 * Unix-like file type, default 0755 permissions for directories or
	 * 0644 for files.
	 */
	sqfs_u16 mode;

	/**
	 * @brief Combination of DIR_ENTRY_FLAG values
	 */
	sqfs_u16 flags;

	/**
	 * @brief Name of the entry
	 *
	 * On Unix-like OSes, the name is returned as-is. On systems like
	 * Windows with encoding-aware APIs, the name is converted to UTF-8.
	 */
	char name[];
} dir_entry_t;

/**
 * @interface dir_iterator_t
 *
 * @brief An iterator over entries in a filesystem directory.
 */
typedef struct dir_iterator_t {
	sqfs_object_t obj;

	/**
	 * @brief Read the next entry and update internal state relating to it
	 *
	 * @param it A pointer to the iterator itself
	 * @param out Returns a pointer to an entry on success
	 *
	 * @return Zero on success, postivie value if the end of the list was
	 *         reached, negative @ref SQFS_ERROR value on failure.
	 */
	int (*next)(struct dir_iterator_t *it, dir_entry_t **out);

	/**
	 * @brief If the last entry was a symlink, extract the target path
	 *
	 * @param it A pointer to the iterator itself.
	 * @param out Returns a pointer to a string on success. Has to be
	 *            released with free().
	 *
	 * @return Zero on success, negative @ref SQFS_ERROR value on failure.
	 */
	int (*read_link)(struct dir_iterator_t *it, char **out);

	/**
	 * @brief If the last entry was a directory, open it.
	 *
	 * If next() returned a directory, this can be used to create a brand
	 * new dir_iterator_t for it, that is independent of the current one
	 * and returns the sub-directories entries.
	 *
	 * @param it A pointer to the iterator itself.
	 * @param out Returns a pointer to a directory iterator on success.
	 *
	 * @return Zero on success, negative @ref SQFS_ERROR value on failure.
	 */
	int (*open_subdir)(struct dir_iterator_t *it,
			   struct dir_iterator_t **out);

	/**
	 * @brief Skip a sub-hierarchy on a stacked iterator
	 *
	 * If an iterator would ordinarily recurse into a sub-directory,
	 * tell it to skip those entries. On simple, flag iterators like the
	 * one returned by @ref dir_iterator_create, this has no effect.
	 *
	 * @param it A pointer to the iterator itself.
	 */
	void (*ignore_subdir)(struct dir_iterator_t *it);

	/**
	 * @brief If the last entry was a regular file, open it.
	 *
	 * If next() returned a file, this can be used to create an istream
	 * to read from it.
	 *
	 * @param it A pointer to the iterator itself.
	 * @param out Returns a pointer to a @ref istream_t on success.
	 *
	 * @return Zero on success, negative @ref SQFS_ERROR value on failure.
	 */
	int (*open_file_ro)(struct dir_iterator_t *it, istream_t **out);

	/**
	 * @brief Read extended attributes associated with the current entry
	 *
	 * @param it A pointer to the iterator itself.
	 * @param out Returns a linked list of xattr entries.
	 *
	 * @return Zero on success, negative @ref SQFS_ERROR value on failure.
	 */
	int (*read_xattr)(struct dir_iterator_t *it, dir_entry_xattr_t **out);
} dir_iterator_t;

enum {
	DIR_SCAN_NO_SOCK = 0x0001,
	DIR_SCAN_NO_SLINK = 0x0002,
	DIR_SCAN_NO_FILE = 0x0004,
	DIR_SCAN_NO_BLK = 0x0008,
	DIR_SCAN_NO_DIR = 0x0010,
	DIR_SCAN_NO_CHR = 0x0020,
	DIR_SCAN_NO_FIFO = 0x0040,

	DIR_SCAN_KEEP_TIME = 0x0100,
	DIR_SCAN_KEEP_UID = 0x0200,
	DIR_SCAN_KEEP_GID = 0x0400,
	DIR_SCAN_KEEP_MODE = 0x0800,

	DIR_SCAN_ONE_FILESYSTEM = 0x1000,
	DIR_SCAN_NO_RECURSION = 0x2000,
	DIR_SCAN_MATCH_FULL_PATH = 0x4000,
};

typedef struct {
	sqfs_u32 flags;
	sqfs_u32 def_uid;
	sqfs_u32 def_gid;
	sqfs_u32 def_mode;
	sqfs_s64 def_mtime;

	/**
	 * @brief A prefix to attach to all returend paths
	 *
	 * If not null, this string and an additional "/" are prepended to
	 * all entries returned by the iterator.
	 */
	const char *prefix;

	/**
	 * @brief A glob pattern that either the name must match
	 *
	 * If this is not NULL, only paths that match this globbing pattern
	 * are returned. If the flag DIR_SCAN_MATCH_FULL_PATH is set, the
	 * entire path must match, slashes cannot match wild card characters.
	 * If not set, only the last part of the path is tested. The iterator
	 * still recurses into directories, it simply doesn't report them if
	 * they don't match.
	 */
	const char *name_pattern;
} dir_tree_cfg_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Construct a simple directory iterator given a path
 *
 * On systems with encoding aware file I/O (like Windows), the path is
 * interpreted to be UTF-8 encoded and converted to the native system API
 * encoding to open the directory. For each directory entry, the name in
 * the native encoding is converted back to UTF-8 when reading.
 *
 * The implementation returned by this is simple, non-recursive, reporting
 * directory contents as returned by the OS native API, i.e. not sorted,
 * and including the "." and ".." entries.
 *
 * @param path A path to a directory on the file system.
 *
 * @return A pointer to a dir_iterator_t implementation on success,
 *         NULL on error (message is printed to stderr).
 */
SQFS_INTERNAL dir_iterator_t *dir_iterator_create(const char *path);

/**
 * @brief Create a stacked, recursive directory tree iterator
 *
 * The underlying implementation automatically recurses into sub directories
 * and returns a flattened list of entries, where each entry represents a full
 * path. Advanced filtering, path pre-fixing et cetera can be configured.
 * The typical "." and ".." entries are filtered out as well.
 *
 * @param path A path to a directory on the file system.
 * @param cfg A @ref dir_tree_cfg_t filtering configuration.
 *
 * @return A pointer to a dir_iterator_t implementation on success,
 *         NULL on error (message is printed to stderr).
 */
SQFS_INTERNAL
dir_iterator_t *dir_tree_iterator_create(const char *path,
					 const dir_tree_cfg_t *cfg);

#ifdef __cplusplus
}
#endif

#endif /* IO_DIR_ITERATOR_H */
