/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"

#include "internal.h"

static uint8_t convert(char in)
{
	if (isupper(in))
		return in - 'A';
	if (islower(in))
		return in - 'a' + 26;
	if (isdigit(in))
		return in - '0' + 52;
	if (in == '+')
		return 62;
	if (in == '/' || in == '-')
		return 63;
	return 0;
}

void base64_decode(uint8_t *out, const char *in)
{
	char temp[4];

	while (*in != '\0' && *in != '=') {
		temp[0] = *in == '\0' ? 0 : convert(*(in++));
		temp[1] = *in == '\0' ? 0 : convert(*(in++));
		temp[2] = *in == '\0' ? 0 : convert(*(in++));
		temp[3] = *in == '\0' ? 0 : convert(*(in++));

		*(out++) = ((temp[0] << 2) & 0xFC) | ((temp[1] >> 4) & 0x03);
		*(out++) = ((temp[1] << 4) & 0xF0) | ((temp[2] >> 2) & 0x0F);
		*(out++) = ((temp[2] << 6) & 0xC0) | ( temp[3]       & 0x3F);
	}
}
