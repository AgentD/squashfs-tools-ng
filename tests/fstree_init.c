/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"

#include "fstree.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

int main(void)
{
	fstree_t fs;
	char *str;

	str = strdup("mtime=1337,uid=1000,gid=100,mode=0321");
	assert(str != NULL);
	assert(fstree_init(&fs, 512, str) == 0);
	free(str);
	assert(fs.defaults.st_mtime == 1337);
	assert(fs.defaults.st_atime == 1337);
	assert(fs.defaults.st_ctime == 1337);
	assert(fs.defaults.st_uid == 1000);
	assert(fs.defaults.st_gid == 100);
	assert(fs.defaults.st_mode == (S_IFDIR | 0321));
	fstree_cleanup(&fs);

	assert(fstree_init(&fs, 512, NULL) == 0);
	assert(fs.defaults.st_mtime == 0);
	assert(fs.defaults.st_atime == 0);
	assert(fs.defaults.st_ctime == 0);
	assert(fs.defaults.st_uid == 0);
	assert(fs.defaults.st_gid == 0);
	assert(fs.defaults.st_mode == (S_IFDIR | 0755));
	fstree_cleanup(&fs);

	str = strdup("mode=07777");
	assert(str != NULL);
	assert(fstree_init(&fs, 512, str) == 0);
	free(str);
	fstree_cleanup(&fs);

	str = strdup("mode=017777");
	assert(str != NULL);
	assert(fstree_init(&fs, 512, str) != 0);
	free(str);

	return EXIT_SUCCESS;
}
