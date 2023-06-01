/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * dir_entry.h
 *
 * Copyright (C) 2023 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef IO_DIR_ENTRY_H
#define IO_DIR_ENTRY_H

#include "sqfs/predef.h"

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
	 * @brief Total size of file entries
	 */
	sqfs_u64 size;

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
	sqfs_u64 uid;

	/**
	 * @brief ID of the group that owns the entry.
	 *
	 * On Windows and other non-Unix OSes, this always reports group 0.
	 */
	sqfs_u64 gid;

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

typedef struct dir_entry_xattr_t {
	struct dir_entry_xattr_t *next;
	char *key;
	sqfs_u8 *value;
	size_t value_len;
	char data[];
} dir_entry_xattr_t;

#ifdef __cplusplus
extern "C" {
#endif

dir_entry_t *dir_entry_create(const char *name);

dir_entry_xattr_t *dir_entry_xattr_create(const char *key, const sqfs_u8 *value,
					  size_t value_len);

dir_entry_xattr_t *dir_entry_xattr_list_copy(const dir_entry_xattr_t *list);

void dir_entry_xattr_list_free(dir_entry_xattr_t *list);

#ifdef __cplusplus
}
#endif

#endif /* IO_DIR_ENTRY_H */
