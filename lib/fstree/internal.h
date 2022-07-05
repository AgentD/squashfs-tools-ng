/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * internal.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef FSTREE_INTERNAL_H
#define FSTREE_INTERNAL_H

#include "config.h"
#include "fstree.h"

void fstree_insert_sorted(tree_node_t *root, tree_node_t *n);

#endif /* FSTREE_INTERNAL_H */
