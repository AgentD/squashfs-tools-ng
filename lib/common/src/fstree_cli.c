/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * fstree_cli.c
 *
 * Copyright (C) 2023 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "common.h"
#include "util/util.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

enum {
	DEF_UID = 0,
	DEF_GID,
	DEF_MODE,
	DEF_MTIME,
};

static const char *defaults[] = {
	[DEF_UID] = "uid",
	[DEF_GID] = "gid",
	[DEF_MODE] = "mode",
	[DEF_MTIME] = "mtime",
	NULL
};

int parse_fstree_defaults(fstree_defaults_t *sb, char *subopts)
{
	char *value;
	long lval;
	int i;

	memset(sb, 0, sizeof(*sb));
	sb->mode = S_IFDIR | 0755;
	sb->mtime = get_source_date_epoch();

	if (subopts == NULL)
		return 0;

	while (*subopts != '\0') {
		i = getsubopt(&subopts, (char *const *)defaults, &value);

		if (value == NULL) {
			fprintf(stderr, "Missing value for option %s\n",
				defaults[i]);
			return -1;
		}

		switch (i) {
		case DEF_UID:
			lval = strtol(value, NULL, 0);
			if (lval < 0)
				goto fail_uv;
			if (lval > (long)INT32_MAX)
				goto fail_ov;
			sb->uid = lval;
			break;
		case DEF_GID:
			lval = strtol(value, NULL, 0);
			if (lval < 0)
				goto fail_uv;
			if (lval > (long)INT32_MAX)
				goto fail_ov;
			sb->gid = lval;
			break;
		case DEF_MODE:
			lval = strtol(value, NULL, 0);
			if (lval < 0)
				goto fail_uv;
			if (lval > 07777)
				goto fail_ov;
			sb->mode = S_IFDIR | (sqfs_u16)lval;
			break;
		case DEF_MTIME:
			lval = strtol(value, NULL, 0);
			if (lval < 0)
				goto fail_uv;
			if (lval > (long)UINT32_MAX)
				goto fail_ov;
			sb->mtime = lval;
			break;
		default:
			fprintf(stderr, "Unknown option '%s'\n", value);
			return -1;
		}
	}
	return 0;
fail_uv:
	fprintf(stderr, "%s: value must be positive\n", defaults[i]);
	return -1;
fail_ov:
	fprintf(stderr, "%s: value too large\n", defaults[i]);
	return -1;
}
