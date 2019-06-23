/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "internal.h"
#include "util.h"

typedef compressor_t *(*compressor_fun_t)(bool compress, size_t block_size,
					  char *options);

typedef void (*compressor_help_fun_t)(void);

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
#ifdef WITH_ZSTD
	[SQFS_COMP_ZSTD] = create_zstd_compressor,
#endif
};

static const compressor_help_fun_t helpfuns[SQFS_COMP_MAX + 1] = {
#ifdef WITH_GZIP
	[SQFS_COMP_GZIP] = compressor_gzip_print_help,
#endif
#ifdef WITH_XZ
	[SQFS_COMP_XZ] = compressor_xz_print_help,
#endif
#ifdef WITH_LZO
	[SQFS_COMP_LZO] = compressor_lzo_print_help,
#endif
#ifdef WITH_LZ4
	[SQFS_COMP_LZ4] = compressor_lz4_print_help,
#endif
#ifdef WITH_ZSTD
	[SQFS_COMP_ZSTD] = compressor_zstd_print_help,
#endif
};

static const char *names[] = {
	[SQFS_COMP_GZIP] = "gzip",
	[SQFS_COMP_LZMA] = "lzma",
	[SQFS_COMP_LZO] = "lzo",
	[SQFS_COMP_XZ] = "xz",
	[SQFS_COMP_LZ4] = "lz4",
	[SQFS_COMP_ZSTD] = "zstd",
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
				size_t block_size, char *options)
{
	if (id < SQFS_COMP_MIN || id > SQFS_COMP_MAX)
		return NULL;

	if (compressors[id] == NULL)
		return NULL;

	return compressors[id](compress, block_size, options);
}

void compressor_print_help(E_SQFS_COMPRESSOR id)
{
	if (id < SQFS_COMP_MIN || id > SQFS_COMP_MAX)
		return;

	if (compressors[id] == NULL)
		return;

	helpfuns[id]();
}

void compressor_print_available(void)
{
	size_t i;

	fputs("Available compressors:\n", stdout);

	for (i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
		if (compressor_exists(i))
			printf("\t%s\n", names[i]);
	}

	printf("\nDefault compressor: %s\n", names[compressor_get_default()]);
}

const char *compressor_name_from_id(E_SQFS_COMPRESSOR id)
{
	if (id < 0 || (size_t)id > sizeof(names) / sizeof(names[0]))
		return NULL;

	return names[id];
}

int compressor_id_from_name(const char *name, E_SQFS_COMPRESSOR *out)
{
	size_t i;

	for (i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
		if (names[i] != NULL && strcmp(names[i], name) == 0) {
			*out = i;
			return 0;
		}
	}

	return -1;
}

E_SQFS_COMPRESSOR compressor_get_default(void)
{
#if defined(WITH_XZ)
	return SQFS_COMP_XZ;
#elif defined(WITH_ZSTD)
	return SQFS_COMP_ZSTD;
#elif defined(WITH_GZIP)
	return SQFS_COMP_GZIP;
#elif defined(WITH_LZO)
	return SQFS_COMP_LZO;
#elif defined(WITH_LZ4)
	return SQFS_COMP_LZ4;
#else
	fputs("No compressor implementation available!\n", stderr);
	exit(EXIT_FAILURE);
#endif
}
