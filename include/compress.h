/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef COMPRESS_H
#define COMPRESS_H

#include <sys/types.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "squashfs.h"

typedef struct compressor_t compressor_t;

/**
 * @struct compressor_t
 *
 * @brief Encapsultes a compressor with a simple interface to compress or
 *        uncompress/extract blocks of data
 */
struct compressor_t {
	/**
	 * @brief Write compressor options to the output file if necessary
	 *
	 * @param cmp A pointer to the compressor object
	 * @param fd  The file descriptor to write to
	 *
	 * @return The number of bytes written or -1 on failure
	 */
	int (*write_options)(compressor_t *cmp, int fd);

	/**
	 * @brief Read compressor options to the input file
	 *
	 * @param cmp A pointer to the compressor object
	 * @param fd  The file descriptor to write to
	 *
	 * @return Zero on success, -1 on failure
	 */
	int (*read_options)(compressor_t *cmp, int fd);

	/**
	 * @brief Compress or uncompress a chunk of data
	 *
	 * @param cmp     A pointer to the compressor object
	 * @param in      A pointer to the input buffer
	 * @param size    The number of bytes in the input buffer to process
	 * @param out     A pointer to the output buffer
	 * @param outsize The number of bytes available in the output buffer
	 *
	 * @return On success, the number of bytes written to the output
	 *         buffer, -1 on failure, 0 if the output buffer was too small.
	 *         The compressor also returns 0 if the compressed result ends
	 *         up larger than the original input.
	 */
	ssize_t (*do_block)(compressor_t *cmp, const uint8_t *in, size_t size,
			    uint8_t *out, size_t outsize);

	/**
	 * @brief Destroy a compressor object and free up all memory it uses
	 *
	 * @param cmp A pointer to the compressor object
	 */
	void (*destroy)(compressor_t *stream);
};

/**
 * @brief Check if a given compressor is available
 *
 * @memberof compressor_t
 *
 * This function checks if a given compressor is available, since some
 * compressors may not be supported yet, or simply disabled in the
 * compile configuration.
 *
 * @param id A SquashFS compressor id
 *
 * @return true if the given compressor is available
 */
bool compressor_exists(E_SQFS_COMPRESSOR id);

/**
 * @brief Create a compressor object
 *
 * @memberof compressor_t
 *
 * @param id         A SquashFS compressor id
 * @param compress   true if the resulting object should compress data, false
 *                   if it should actually extract already compressed blocks.
 * @param block_size The configured block size for the SquashFS image. May be
 *                   of interest to some compressors.
 *
 * @return A pointer to a new compressor object or NULL on failure.
 */
compressor_t *compressor_create(E_SQFS_COMPRESSOR id, bool compress,
				size_t block_size);

#endif /* COMPRESS_H */
