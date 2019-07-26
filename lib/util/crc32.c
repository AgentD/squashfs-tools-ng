/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include "util.h"

/*
  Karl Malbrain's compact CRC-32. See "A compact CCITT crc16 and crc32 C
  implementation that balances processor cache usage against speed"
 */
static const uint32_t s_crc32[16] = {
	0x00000000, 0x1DB71064, 0x3B6E20C8, 0x26D930AC,
	0x76DC4190, 0x6B6B51F4, 0x4DB26158, 0x5005713C,
	0xEDB88320, 0xF00F9344, 0xD6D6A3E8, 0xCB61B38C,
	0x9B64C2B0, 0x86D3D2D4, 0xA00AE278, 0xBDBDF21C
};

uint32_t update_crc32(uint32_t crc, const void *data, size_t size)
{
	const uint8_t *ptr = data;
	uint8_t b;

	crc = ~crc;

	while (size--) {
		b = *ptr++;

		crc = (crc >> 4) ^ s_crc32[(crc & 0x0F) ^ (b & 0x0F)];
		crc = (crc >> 4) ^ s_crc32[(crc & 0x0F) ^ (b >> 4)];
	}

	return ~crc;
}
