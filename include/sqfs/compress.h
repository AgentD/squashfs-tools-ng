/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * compress.h - This file is part of libsquashfs
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#ifndef SQFS_COMPRESS_H
#define SQFS_COMPRESS_H

#include "sqfs/predef.h"
#include "sqfs/super.h"

/**
 * @file compress.h
 *
 * @brief Contains declarations to everything related to data compression.
 */

/**
 * @interface sqfs_compressor_t
 *
 * @brief Encapsultes a compressor with a simple interface to compress or
 *        extract chunks of data.
 */
struct sqfs_compressor_t {
	/**
	 * @brief Destroy a compressor and free all memory used by it.
	 *
	 * @param cmp A pointer to a compressor object.
	 */
	void (*destroy)(sqfs_compressor_t *cmp);

	/**
	 * @brief Write compressor options to disk if non-default settings
	 *        have been used.
	 *
	 * The options are stored in an uncompressed meta data block directly
	 * after the super block.
	 *
	 * @param cmp A pointer to a compressor object.
	 * @param file A file to write to.
	 *
	 * @return The number of bytes written on success, 0 means default
	 *         settings are used. A negative value is an @ref E_SQFS_ERROR
	 *         identifier.
	 */
	int (*write_options)(sqfs_compressor_t *cmp, sqfs_file_t *file);

	/**
	 * @brief Read compressor options from disk.
	 *
	 * @param cmp A pointer to a compressor object.
	 * @param file A file to read from.
	 *
	 * @return Zero on success or an @ref E_SQFS_ERROR value.
	 */
	int (*read_options)(sqfs_compressor_t *cmp, sqfs_file_t *file);

	/**
	 * @brief Compress or uncompress a chunk of data.
	 *
	 * @param cmp A pointer to a compressor object.
	 * @param in A pointer to the input buffer to read from.
	 * @param size The number of bytes to read from the input and compress.
	 * @param out The destination buffer to write the result to.
	 * @param outsize The available space in the destination buffer.
	 *
	 * @return The number of bytes written to the buffer, a negative
	 *         value is an @ref E_SQFS_ERROR value. The value 0 means
	 *         the output buffer was too small when extracting or that
	 *         the result is larger than the input when compressing.
	 */
	ssize_t (*do_block)(sqfs_compressor_t *cmp, const sqfs_u8 *in,
			    size_t size, sqfs_u8 *out, size_t outsize);

	/**
	 * @brief Create an exact copt of agiven compressor
	 *
	 * @param cmp A pointer to a compressor object.
	 *
	 * @return A deep copy of the given compressor.
	 */
	sqfs_compressor_t *(*create_copy)(sqfs_compressor_t *cmp);
};

/**
 * @struct sqfs_compressor_config_t
 *
 * @brief Configuration parameters for instantiating a compressor backend.
 */
struct sqfs_compressor_config_t {
	/**
	 * @brief An @ref E_SQFS_COMPRESSOR identifier
	 */
	sqfs_u16 id;

	/**
	 * @brief A combination of @ref SQFS_COMP_FLAG flags.
	 */
	sqfs_u16 flags;

	/**
	 * @brief The intended data block size.
	 */
	sqfs_u32 block_size;

	/**
	 * @brief Backend specific options for fine tuing.
	 */
	union {
		/**
		 * @brief Options for the zlib compressor.
		 */
		struct {
			/**
			 * @brief Compression level. Value between 1 and 9.
			 *
			 * Default is 9, i.e. best compression.
			 */
			sqfs_u16 level;

			/**
			 * @brief Deflate window size. Value between 8 and 15.
			 *
			 * Default is 15, i.e. 32k window.
			 */
			sqfs_u16 window_size;
		} gzip;

		/**
		 * @brief Options for the zstd compressor.
		 */
		struct {
			/**
			 * @brief Compression level. Value between 1 and 22.
			 *
			 * Default is 15.
			 */
			sqfs_u16 level;
		} zstd;

		/**
		 * @brief Options for the lzo compressor.
		 */
		struct {
			/**
			 * @brief Which variant of lzo should be used.
			 *
			 * An @ref SQFS_LZO_ALGORITHM value. Default is
			 * @ref SQFS_LZO1X_999, i.e. best compression.
			 */
			sqfs_u16 algorithm;

			/**
			 * @brief Compression level for @ref SQFS_LZO1X_999.
			 *
			 * If the selected algorithm is @ref SQFS_LZO1X_999,
			 * this can be a value between 0 and 9. For all other
			 * algorithms it has to be 0.
			 *
			 * Defaults to 9, i.e. best compression.
			 */
			sqfs_u16 level;
		} lzo;

		/**
		 * @brief Options for the xz compressor.
		 */
		struct {
			/**
			 * @brief LZMA dictionary size.
			 *
			 * This value must either be a power of two or the sumo
			 * of two consecutive powers of two.
			 *
			 * Default is setting this to the same as the
			 * block size.
			 */
			sqfs_u32 dict_size;
		} xz;
	} opt;
};

/**
 * @enum SQFS_COMP_FLAG
 *
 * @brief Flags for configuring the compressor.
 */
typedef enum {
	/**
	 * @brief For LZ4, set this to use high compression mode.
	 */
	SQFS_COMP_FLAG_LZ4_HC = 0x0001,
	SQFS_COMP_FLAG_LZ4_ALL = 0x0001,

	/**
	 * @brief For LZMA, set this to select the x86 BCJ filter.
	 */
	SQFS_COMP_FLAG_XZ_X86 = 0x0001,

	/**
	 * @brief For LZMA, set this to select the PowerPC BCJ filter.
	 */
	SQFS_COMP_FLAG_XZ_POWERPC = 0x0002,

	/**
	 * @brief For LZMA, set this to select the Itanium BCJ filter.
	 */
	SQFS_COMP_FLAG_XZ_IA64 = 0x0004,

	/**
	 * @brief For LZMA, set this to select the ARM BCJ filter.
	 */
	SQFS_COMP_FLAG_XZ_ARM = 0x0008,

	/**
	 * @brief For LZMA, set this to select the ARM Thumb BCJ filter.
	 */
	SQFS_COMP_FLAG_XZ_ARMTHUMB = 0x0010,

	/**
	 * @brief For LZMA, set this to select the Sparc BCJ filter.
	 */
	SQFS_COMP_FLAG_XZ_SPARC = 0x0020,
	SQFS_COMP_FLAG_XZ_ALL = 0x003F,

	/**
	 * @brief For zlib deflate, set this to try the default strategy.
	 */
	SQFS_COMP_FLAG_GZIP_DEFAULT = 0x0001,

	/**
	 * @brief For zlib deflate, set this to try the "filtered" strategy.
	 */
	SQFS_COMP_FLAG_GZIP_FILTERED = 0x0002,

	/**
	 * @brief For zlib deflate, set this to try the huffman only strategy.
	 */
	SQFS_COMP_FLAG_GZIP_HUFFMAN = 0x0004,

	/**
	 * @brief For zlib deflate, set this to try the RLE strategy.
	 */
	SQFS_COMP_FLAG_GZIP_RLE = 0x0008,

	/**
	 * @brief For zlib deflate, set this to try the fixed strategy.
	 */
	SQFS_COMP_FLAG_GZIP_FIXED = 0x0010,
	SQFS_COMP_FLAG_GZIP_ALL = 0x001F,

	/**
	 * @brief Set this if the compressor should actually extract
	 *        instead of compress data.
	 */
	SQFS_COMP_FLAG_UNCOMPRESS = 0x8000,
	SQFS_COMP_FLAG_GENERIC_ALL = 0x8000,
} SQFS_COMP_FLAG;

/**
 * @enum SQFS_LZO_ALGORITHM
 *
 * @brief The available LZO algorithms.
 */
typedef enum {
	SQFS_LZO1X_1 = 0,
	SQFS_LZO1X_1_11 = 1,
	SQFS_LZO1X_1_12 = 2,
	SQFS_LZO1X_1_15 = 3,
	SQFS_LZO1X_999	= 4,
} SQFS_LZO_ALGORITHM;

#define SQFS_GZIP_DEFAULT_LEVEL (9)
#define SQFS_GZIP_DEFAULT_WINDOW (15)

#define SQFS_LZO_DEFAULT_ALG SQFS_LZO1X_999
#define SQFS_LZO_DEFAULT_LEVEL (8)

#define SQFS_ZSTD_DEFAULT_LEVEL (15)

#define SQFS_GZIP_MIN_LEVEL (1)
#define SQFS_GZIP_MAX_LEVEL (9)

#define SQFS_LZO_MIN_LEVEL (0)
#define SQFS_LZO_MAX_LEVEL (9)

#define SQFS_ZSTD_MIN_LEVEL (1)
#define SQFS_ZSTD_MAX_LEVEL (22)

#define SQFS_GZIP_MIN_WINDOW (8)
#define SQFS_GZIP_MAX_WINDOW (15)

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize a compressor configuration
 *
 * The detail configuration options are all initialized to the defaults for
 * the compressor in question.
 *
 * @param cfg A pointer to a compressor configuration to initialize
 * @param id The compressor id to set.
 * @param block_size The block size to set.
 * @param flags The compressor flags to set.
 *
 * @return Zero on success, an @ref E_SQFS_ERROR value if some of the options
 *         don't make sense (e.g. unknown flags are used).
 */
SQFS_API int sqfs_compressor_config_init(sqfs_compressor_config_t *cfg,
					 E_SQFS_COMPRESSOR id,
					 size_t block_size, sqfs_u16 flags);

/**
 * @brief Check if a specified compressor implementation is available.
 *
 * @param id An @ref E_SQFS_COMPRESSOR identifier.
 *
 * @return true if the implementation is available and can be instantiated
 *         through @ref sqfs_compressor_create.
 */
SQFS_API bool sqfs_compressor_exists(E_SQFS_COMPRESSOR id);

/**
 * @brief Create an instance of a compressor implementation.
 *
 * @param cfg A pointer to a compressor configuration.
 *
 * @return A pointer to a compressor object on success, NULL on allocation
 *         failure or if initializing the compressor failed.
 */
SQFS_API
sqfs_compressor_t *sqfs_compressor_create(const sqfs_compressor_config_t *cfg);

/**
 * @brief Get the name of a compressor backend from its ID.
 *
 * This function will even resolve compressor names that are not built in, so
 * use @ref sqfs_compressor_exists to check if a compressor is actually
 * available.
 *
 * @param id An @ref E_SQFS_COMPRESSOR identifier.
 *
 * @return A string holding the name of the compressor, NULL if the compressor
 *         ID is not known.
 */
SQFS_API const char *sqfs_compressor_name_from_id(E_SQFS_COMPRESSOR id);

/**
 * @brief Get the compressor ID using just the name of the backend.
 *
 * This function will even resolve compressor names that are not built in, so
 * use @ref sqfs_compressor_exists to check if a compressor is actually
 * available.
 *
 * @param name The name of the compressor backend.
 * @param out Returns the coresponding @ref E_SQFS_COMPRESSOR identifier.
 *
 * @return Zero on success, -1 if the backend is unknown.
 */
SQFS_API
int sqfs_compressor_id_from_name(const char *name, E_SQFS_COMPRESSOR *out);

#ifdef __cplusplus
}
#endif

#endif /* SQFS_COMPRESS_H */
