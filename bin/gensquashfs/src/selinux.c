/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * selinux.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "mkfs.h"

#define XATTR_NAME_SELINUX "security.selinux"
#define XATTR_VALUE_SELINUX "system_u:object_r:unlabeled_t:s0"

#ifdef WITH_SELINUX
int selinux_relable_node(void *sehnd, sqfs_xattr_writer_t *xwr,
			 tree_node_t *node, const char *path)
{
	char *context = NULL;
	int ret;

	if (selabel_lookup(sehnd, &context, path, node->mode) < 0) {
		context = strdup(XATTR_VALUE_SELINUX);
		if (context == NULL)
			goto fail;
	}

	ret = sqfs_xattr_writer_add_kv(xwr, XATTR_NAME_SELINUX,
				       context, strlen(context));
	free(context);

	if (ret)
		sqfs_perror(node->name, "storing SELinux xattr", ret);

	return ret;
fail:
	perror("relabeling files");
	return -1;
}

void *selinux_open_context_file(const char *filename)
{
	struct selabel_handle *sehnd;
	struct selinux_opt seopts[] = {
		{ SELABEL_OPT_PATH, filename },
	};

	sehnd = selabel_open(SELABEL_CTX_FILE, seopts, 1);
	if (sehnd == NULL)
		perror(filename);

	return sehnd;
}

void selinux_close_context_file(void *sehnd)
{
	selabel_close(sehnd);
}
#else
int selinux_relable_node(void *sehnd, sqfs_xattr_writer_t *xwr,
			 tree_node_t *node, const char *path)
{
	(void)sehnd; (void)xwr; (void)node; (void)path;
	fputs("Built without SELinux support, cannot add SELinux labels\n",
	      stderr);
	return -1;
}

void *selinux_open_context_file(const char *filename)
{
	(void)filename;
	fputs("Built without SELinux support, cannot open contexts file\n",
	      stderr);
	return NULL;
}

void selinux_close_context_file(void *sehnd)
{
	(void)sehnd;
}
#endif
