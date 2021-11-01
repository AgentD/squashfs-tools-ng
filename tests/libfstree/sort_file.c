/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * sort_file.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "sqfs/block.h"
#include "fstree.h"
#include "../test.h"

static const char *listing =
"dir /bin 0755 0 0\n"
"dir /lib 0755 0 0\n"
"dir /usr 0755 0 0\n"
"dir /usr/share 0755 0 0\n"
"\n"
"file /bin/chown 0755 0 0\n"
"file /bin/ls 0755 0 0\n"
"file /bin/chmod 0755 0 0\n"
"file /bin/dir 0755 0 0\n"
"file /bin/cp 0755 0 0\n"
"file /bin/dd 0755 0 0\n"
"file /bin/ln 0755 0 0\n"
"file /bin/mkdir 0755 0 0\n"
"file /bin/mknod 0755 0 0\n"
"\n"
"file /lib/libssl.so 0755 0 0\n"
"file /lib/libfoobar.so 0755 0 0\n"
"file /lib/libwhatever.so 0755 0 0\n"
"\n"
"file /usr/share/bla.txt 0644 0 0\n";

static const char *sort_file =
"# Blockwise reverse the order of the /bin files\n"
"  10 [glob] /bin/mk*\n"
"  20 [glob] /bin/ch*\n"
"  30 [glob] /bin/d*\n"
"  40        /bin/cp\n"
"  50 [glob] /bin/*\n"
"\n"
"# Make this file appear first\n"
"  -10000 [dont_compress,dont_fragment,align] /usr/share/bla.txt";

static const char *initial_order[] = {
	"bin/chmod",
	"bin/chown",
	"bin/cp",
	"bin/dd",
	"bin/dir",
	"bin/ln",
	"bin/ls",
	"bin/mkdir",
	"bin/mknod",
	"lib/libfoobar.so",
	"lib/libssl.so",
	"lib/libwhatever.so",
	"usr/share/bla.txt",
};

static const char *after_sort_order[] = {
	"usr/share/bla.txt",
	"lib/libfoobar.so",
	"lib/libssl.so",
	"lib/libwhatever.so",
	"bin/mkdir",
	"bin/mknod",
	"bin/chmod",
	"bin/chown",
	"bin/dd",
	"bin/dir",
	"bin/cp",
	"bin/ln",
	"bin/ls",
};

static sqfs_s64 priorities[] = {
	-10000,
	0,
	0,
	0,
	10,
	10,
	20,
	20,
	30,
	30,
	40,
	50,
	50,
};

static int flags[] = {
	SQFS_BLK_DONT_COMPRESS | SQFS_BLK_ALIGN | SQFS_BLK_DONT_FRAGMENT,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
};

/*****************************************************************************/

static sqfs_u8 temp_buffer[2048];
static const char *input_file = NULL;

static void destroy_noop(sqfs_object_t *obj)
{
	(void)obj;
}

static int memfile_load(istream_t *strm)
{
	strcpy((char *)temp_buffer, input_file);
	strm->eof = true;
	strm->buffer_used = strlen(input_file);
	return 0;
}

static const char *get_filename(istream_t *strm)
{
	(void)strm;
	return "memstream";
}

static istream_t memstream = {
	.base = {
		.destroy = destroy_noop,
	},

	.buffer_used = 0,
	.buffer_offset = 0,
	.eof = false,
	.buffer = temp_buffer,

	.precache = memfile_load,
	.get_filename = get_filename,
};

/*****************************************************************************/

int main(void)
{
	file_info_t *fi;
	fstree_t fs;
	size_t i;

	input_file = listing;
	memstream.buffer_used = 0;
	memstream.buffer_offset = 0;
	memstream.eof = false;

	TEST_ASSERT(fstree_init(&fs, NULL) == 0);
	TEST_ASSERT(fstree_from_file_stream(&fs, &memstream, NULL) == 0);

	fstree_post_process(&fs);

	for (i = 0, fi = fs.files; fi != NULL; fi = fi->next, ++i) {
		tree_node_t *n = container_of(fi, tree_node_t, data.file);
		char *path = fstree_get_path(n);
		int ret;

		TEST_NOT_NULL(path);

		ret = canonicalize_name(path);
		TEST_EQUAL_I(ret, 0);

		TEST_STR_EQUAL(initial_order[i], path);
		free(path);

		TEST_EQUAL_I(fi->priority, 0);
		TEST_EQUAL_I(fi->flags, 0);
	}

	TEST_EQUAL_UI(i, sizeof(initial_order) / sizeof(initial_order[0]));


	input_file = sort_file;
	memstream.buffer_used = 0;
	memstream.buffer_offset = 0;
	memstream.eof = false;

	TEST_ASSERT(fstree_sort_files(&fs, &memstream) == 0);

	for (i = 0, fi = fs.files; fi != NULL; fi = fi->next, ++i) {
		tree_node_t *n = container_of(fi, tree_node_t, data.file);
		char *path = fstree_get_path(n);
		int ret;

		TEST_NOT_NULL(path);

		ret = canonicalize_name(path);
		TEST_EQUAL_I(ret, 0);

		TEST_STR_EQUAL(after_sort_order[i], path);
		free(path);

		TEST_EQUAL_I(fi->priority, priorities[i]);
		TEST_EQUAL_I(fi->flags, flags[i]);
	}

	TEST_EQUAL_UI(i, sizeof(after_sort_order) /
		      sizeof(after_sort_order[0]));

	fstree_cleanup(&fs);
	return EXIT_SUCCESS;
}
