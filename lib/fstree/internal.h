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

/* ASCIIbetically sort a linked list of tree nodes */
tree_node_t *tree_node_list_sort(tree_node_t *head);

/*
  If the environment variable SOURCE_DATE_EPOCH is set to a parsable number
  that fits into an unsigned 32 bit value, return its value. Otherwise,
  default to 0.
 */
sqfs_u32 get_source_date_epoch(void);

#endif /* FSTREE_INTERNAL_H */
