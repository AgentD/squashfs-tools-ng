/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <string.h>

#include "internal.h"

typedef compressor_t *(*compressor_fun_t)(bool compress, size_t block_size);

static compressor_fun_t compressors[SQFS_COMP_MAX + 1] = {
#ifdef WITH_GZIP
	[SQFS_COMP_GZIP] = create_gzip_compressor,
#endif
#ifdef WITH_XZ
	[SQFS_COMP_XZ] = create_xz_compressor,
#endif
};

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
