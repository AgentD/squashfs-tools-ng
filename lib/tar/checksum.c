/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * checksum.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "internal.h"

static unsigned int get_checksum(const tar_header_t *hdr)
{
	const unsigned char *header_start = (const unsigned char *)hdr;
	const unsigned char *chksum_start = (const unsigned char *)hdr->chksum;
	const unsigned char *header_end = header_start + sizeof(*hdr);
	const unsigned char *chksum_end = chksum_start + sizeof(hdr->chksum);
	const unsigned char *p;
	unsigned int chksum = 0;

	for (p = header_start; p < chksum_start; p++)
		chksum += *p;
	for (; p < chksum_end; p++)
		chksum += ' ';
	for (; p < header_end; p++)
		chksum += *p;
	return chksum;
}

void update_checksum(tar_header_t *hdr)
{
	unsigned int chksum = get_checksum(hdr);

	sprintf(hdr->chksum, "%06o", chksum);
	hdr->chksum[6] = '\0';
	hdr->chksum[7] = ' ';
}

bool is_checksum_valid(const tar_header_t *hdr)
{
	unsigned int calculated_chksum = get_checksum(hdr);
	uint64_t read_chksum;

	if (read_octal(hdr->chksum, sizeof(hdr->chksum), &read_chksum))
		return 0;
	return read_chksum == calculated_chksum;
}
