/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * sort_file.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "sqfs/block.h"
#include "util/test.h"
#include "util/util.h"
#include "mkfs.h"

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
"  -10000 [dont_compress,dont_fragment] /usr/share/bla.txt";

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
	SQFS_BLK_DONT_COMPRESS | SQFS_BLK_DONT_FRAGMENT,
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

int main(int argc, char **argv)
{
	sqfs_istream_t *memstream;
	fstree_defaults_t fsd;
	tree_node_t *n;
	options_t opt;
	fstree_t fs;
	size_t i;
	(void)argc; (void)argv;

	memset(&opt, 0, sizeof(opt));

	memstream = istream_memory_create("listing.txt", 1024,
					  listing, strlen(listing));
	TEST_NOT_NULL(memstream);

	TEST_ASSERT(parse_fstree_defaults(&fsd, NULL) == 0);
	TEST_ASSERT(fstree_init(&fs, &fsd) == 0);
	TEST_ASSERT(fstree_from_file_stream(&fs, memstream, &opt) == 0);
	sqfs_drop(memstream);

	fstree_post_process(&fs);

	for (i = 0, n = fs.files; n != NULL; n = n->next_by_type, ++i) {
		char *path = fstree_get_path(n);
		int ret;

		TEST_NOT_NULL(path);

		ret = canonicalize_name(path);
		TEST_EQUAL_I(ret, 0);

		TEST_STR_EQUAL(initial_order[i], path);
		free(path);

		TEST_EQUAL_I(n->data.file.priority, 0);
		TEST_EQUAL_I(n->data.file.flags, 0);
	}

	TEST_EQUAL_UI(i, sizeof(initial_order) / sizeof(initial_order[0]));


	memstream = istream_memory_create("sortfile.txt", 1024,
					  sort_file, strlen(sort_file));
	TEST_NOT_NULL(memstream);
	TEST_ASSERT(fstree_sort_files(&fs, memstream) == 0);
	sqfs_drop(memstream);

	for (i = 0, n = fs.files; n != NULL; n = n->next_by_type, ++i) {
		char *path = fstree_get_path(n);
		int ret;

		TEST_NOT_NULL(path);

		ret = canonicalize_name(path);
		TEST_EQUAL_I(ret, 0);

		TEST_STR_EQUAL(after_sort_order[i], path);
		free(path);

		TEST_EQUAL_I(n->data.file.priority, priorities[i]);
		TEST_EQUAL_I(n->data.file.flags, flags[i]);
	}

	TEST_EQUAL_UI(i, sizeof(after_sort_order) /
		      sizeof(after_sort_order[0]));

	fstree_cleanup(&fs);
	return EXIT_SUCCESS;
}
