/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * checksum.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "tar/format.h"

unsigned int tar_compute_checksum(const tar_header_t *hdr)
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
