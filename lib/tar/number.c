/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * number.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "internal.h"

int read_octal(const char *str, int digits, uint64_t *out)
{
	uint64_t result = 0;

	while (digits > 0 && isspace(*str)) {
		++str;
		--digits;
	}

	while (digits > 0 && *str >= '0' && *str <= '7') {
		if (result > 0x1FFFFFFFFFFFFFFFUL) {
			fputs("numeric overflow parsing tar header\n", stderr);
			return -1;
		}

		result = (result << 3) | (*(str++) - '0');
		--digits;
	}

	*out = result;
	return 0;
}

int read_binary(const char *str, int digits, uint64_t *out)
{
	uint64_t x, ov, result = 0;
	bool first = true;

	while (digits > 0) {
		x = *((const unsigned char *)str++);
		--digits;

		if (first) {
			first = false;
			if (x == 0xFF) {
				result = 0xFFFFFFFFFFFFFFFFUL;
			} else {
				x &= 0x7F;
				result = 0;
				if (digits > 7 && x != 0)
					goto fail_ov;
			}
		}

		ov = (result >> 56) & 0xFF;

		if (ov != 0 && ov != 0xFF)
			goto fail_ov;

		result = (result << 8) | x;
	}

	*out = result;
	return 0;
fail_ov:
	fputs("numeric overflow parsing tar header\n", stderr);
	return -1;
}

int read_number(const char *str, int digits, uint64_t *out)
{
	if (*((unsigned char *)str) & 0x80)
		return read_binary(str, digits, out);

	return read_octal(str, digits, out);
}

int pax_read_decimal(const char *str, uint64_t *out)
{
	uint64_t result = 0;

	while (*str >= '0' && *str <= '9') {
		if (result > 0xFFFFFFFFFFFFFFFFUL / 10) {
			fputs("numeric overflow parsing pax header\n", stderr);
			return -1;
		}

		result = (result * 10) + (*(str++) - '0');
	}

	*out = result;
	return 0;
}
