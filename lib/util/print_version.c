/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include "util.h"

#include <stdio.h>

#define LICENSE_SHORT "GPLv3+"
#define LICENSE_LONG "GNU GPL version 3 or later"
#define LICENSE_URL "https://gnu.org/licenses/gpl.html"

extern char *__progname;

static const char *version_string =
"%s (%s) %s\n"
"Copyright (c) 2019 David Oberhollenzer\n"
"License " LICENSE_SHORT ": " LICENSE_LONG " <" LICENSE_URL ">.\n"
"This is free software: you are free to change and redistribute it.\n"
"There is NO WARRANTY, to the extent permitted by law.\n"
"\n"
"Written by David Oberhollenzer.\n";

void print_version(void)
{
	printf(version_string, __progname, PACKAGE_NAME, PACKAGE_VERSION);
}
