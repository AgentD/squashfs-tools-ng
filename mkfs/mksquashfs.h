/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef MKSQUASHFS_H
#define MKSQUASHFS_H

#include "squashfs.h"
#include "options.h"
#include "fstree.h"

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>

int sqfs_super_init(sqfs_super_t *s, const options_t *opt);

int sqfs_padd_file(sqfs_super_t *s, const options_t *opt, int outfd);

int sqfs_super_write(const sqfs_super_t *super, int outfd);

#endif /* MKSQUASHFS_H */
