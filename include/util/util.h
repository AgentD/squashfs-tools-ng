/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * util.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef UTIL_H
#define UTIL_H

#include "config.h"
#include "sqfs/predef.h"

#include <stdint.h>
#include <stddef.h>

#include "compat.h"

#define container_of(ptr, type, member) \
	((type *)((char *)ptr - offsetof(type, member)))

#endif /* UTIL_H */
