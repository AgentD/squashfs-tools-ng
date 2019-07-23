/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"

#include "internal.h"

void update_checksum(tar_header_t *hdr)
{
	unsigned int chksum = 0;
	size_t i;

	memset(hdr->chksum, ' ', sizeof(hdr->chksum));

	for (i = 0; i < sizeof(*hdr); ++i)
		chksum += ((unsigned char *)hdr)[i];

	sprintf(hdr->chksum, "%06o", chksum);
	hdr->chksum[6] = '\0';
	hdr->chksum[7] = ' ';
}

bool is_checksum_valid(const tar_header_t *hdr)
{
	tar_header_t copy;

	memcpy(&copy, hdr, sizeof(*hdr));
	update_checksum(&copy);
	return memcmp(hdr, &copy, sizeof(*hdr)) == 0;
}
