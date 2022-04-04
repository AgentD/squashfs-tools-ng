/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * find_by_path.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "internal.h"

int sqfs_dir_reader_find_by_path(sqfs_dir_reader_t *rd,
				 const sqfs_inode_generic_t *start,
				 const char *path, sqfs_inode_generic_t **out)
{
	sqfs_inode_generic_t *inode;
	sqfs_dir_entry_t *ent;
	const char *ptr;
	int ret = 0;

	if (start == NULL) {
		ret = sqfs_dir_reader_get_root_inode(rd, &inode);
	} else {
		inode = alloc_flex(sizeof(*inode), 1,
				   start->payload_bytes_used);
		if (inode == NULL) {
			ret = SQFS_ERROR_ALLOC;
		} else {
			memcpy(inode, start,
			       sizeof(*start) + start->payload_bytes_used);
		}
	}

	if (ret)
		return ret;

	while (*path != '\0') {
		if (*path == '/') {
			while (*path == '/')
				++path;
			continue;
		}

		ret = sqfs_dir_reader_open_dir(rd, inode, 0);
		free(inode);
		if (ret)
			return ret;

		ptr = strchr(path, '/');
		if (ptr == NULL) {

			if (ptr == NULL) {
				for (ptr = path; *ptr != '\0'; ++ptr)
					;
			}
		}

		do {
			ret = sqfs_dir_reader_read(rd, &ent);
			if (ret < 0)
				return ret;

			if (ret == 0) {
				ret = strncmp((const char *)ent->name,
					      path, ptr - path);
				if (ret == 0)
					ret = ent->name[ptr - path];
				free(ent);
			}
		} while (ret < 0);

		if (ret > 0)
			return SQFS_ERROR_NO_ENTRY;

		ret = sqfs_dir_reader_get_inode(rd, &inode);
		if (ret)
			return ret;

		path = ptr;
	}

	*out = inode;
	return 0;
}
