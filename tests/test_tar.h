/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * test_tar.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef TEST_TAR_H
#define TEST_TAR_H

#include "config.h"
#include "tar.h"
#include "test.h"

#define STR(x) #x
#define STRVALUE(x) STR(x)

#define TEST_PATH STRVALUE(TESTPATH)

#endif /* TEST_TAR_H */
