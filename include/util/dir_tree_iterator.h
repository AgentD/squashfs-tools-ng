/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * dir_tree_iterator.h
 *
 * Copyright (C) 2023 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef UTIL_DIR_TREE_ITERATOR_H
#define UTIL_DIR_TREE_ITERATOR_H

#include "util/dir_iterator.h"

dir_iterator_t *dir_tree_iterator_create(const char *path);

void dir_tree_iterator_skip(dir_iterator_t *it);

#endif /* UTIL_DIR_TREE_ITERATOR_H */
