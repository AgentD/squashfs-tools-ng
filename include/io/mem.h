/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * mem.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef IO_MEM_STREAM_H
#define IO_MEM_STREAM_H

#include "io/istream.h"

#ifdef __cplusplus
extern "C" {
#endif

SQFS_INTERNAL istream_t *istream_memory_create(const char *name, size_t bufsz,
					       const void *data, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* IO_MEM_STREAM_H */
