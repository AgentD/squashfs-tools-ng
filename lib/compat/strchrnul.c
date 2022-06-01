#include "config.h"
#include "compat.h"

#ifndef HAVE_STRCHRNUL
char *strchrnul(const char *s, int c)
{
	while (*s && *((unsigned char *)s) != c)
		++s;

	return (char *)s;
}
#endif
