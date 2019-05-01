/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef ID_TABLE_H
#define ID_TABLE_H

#include <stdint.h>
#include <stddef.h>

#include "compress.h"

typedef struct {
	uint32_t *ids;
	size_t num_ids;
	size_t max_ids;
} id_table_t;

int id_table_init(id_table_t *tbl);

void id_table_cleanup(id_table_t *tbl);

int id_table_id_to_index(id_table_t *tbl, uint32_t id, uint16_t *out);

int id_table_write(id_table_t *tbl, int outfd, sqfs_super_t *super,
		   compressor_t *cmp);

#endif /* ID_TABLE_H */
