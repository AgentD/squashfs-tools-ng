/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <string.h>
#include <stdio.h>

#include "internal.h"
#include "util.h"

typedef compressor_t *(*compressor_fun_t)(bool compress, size_t block_size);

static compressor_fun_t compressors[SQFS_COMP_MAX + 1] = {
#ifdef WITH_GZIP
	[SQFS_COMP_GZIP] = create_gzip_compressor,
#endif
#ifdef WITH_XZ
	[SQFS_COMP_XZ] = create_xz_compressor,
#endif
#ifdef WITH_LZO
	[SQFS_COMP_LZO] = create_lzo_compressor,
#endif
#ifdef WITH_LZ4
	[SQFS_COMP_LZ4] = create_lz4_compressor,
#endif
};

int generic_write_options(int fd, const void *data, size_t size)
{
	uint8_t buffer[size + 2];
	ssize_t ret;

	*((uint16_t *)buffer) = htole16(0x8000 | size);
	memcpy(buffer + 2, data, size);

	ret = write_retry(fd, buffer, sizeof(buffer));

	if (ret < 0) {
		perror("writing compressor options");
		return -1;
	}

	if ((size_t)ret < sizeof(buffer)) {
		fputs("writing compressor options: truncated write\n", stderr);
		return -1;
	}

	return ret;
}

int generic_read_options(int fd, void *data, size_t size)
{
	uint8_t buffer[size + 2];
	ssize_t ret;

	ret = read_retry(fd, buffer, sizeof(buffer));

	if (ret < 0) {
		perror("reading compressor options");
		return -1;
	}

	if ((size_t)ret < sizeof(buffer)) {
		fputs("reading compressor options: unexpected end of file\n",
		      stderr);
		return -1;
	}

	if (le16toh(*((uint16_t *)buffer)) != (0x8000 | size)) {
		fputs("reading compressor options: invalid meta data header\n",
		      stderr);
		return -1;
	}

	memcpy(data, buffer + 2, size);
	return 0;
}

bool compressor_exists(E_SQFS_COMPRESSOR id)
{
	if (id < SQFS_COMP_MIN || id > SQFS_COMP_MAX)
		return false;

	return (compressors[id] != NULL);
}

compressor_t *compressor_create(E_SQFS_COMPRESSOR id, bool compress,
				size_t block_size)
{
	if (id < SQFS_COMP_MIN || id > SQFS_COMP_MAX)
		return NULL;

	if (compressors[id] == NULL)
		return NULL;

	return compressors[id](compress, block_size);
}
