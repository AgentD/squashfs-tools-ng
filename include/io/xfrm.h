/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * xfrm.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef IO_XFRM_H
#define IO_XFRM_H

#include "io/istream.h"
#include "io/ostream.h"

enum {
	/**
	 * @brief Deflate compressor with gzip headers.
	 *
	 * This actually creates a gzip compatible file, including a
	 * gzip header and trailer.
	 */
	IO_COMPRESSOR_GZIP = 1,

	IO_COMPRESSOR_XZ = 2,

	IO_COMPRESSOR_ZSTD = 3,

	IO_COMPRESSOR_BZIP2 = 4,

	IO_COMPRESSOR_MIN = 1,
	IO_COMPRESSOR_MAX = 4,
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create an input stream that transparently uncompresses data.
 *
 * @memberof istream_t
 *
 * This function creates an input stream that wraps an underlying input stream
 * that is compressed and transparently uncompresses the data when reading
 * from it.
 *
 * The new stream takes ownership of the wrapped stream and destroys it when
 * the compressor stream is destroyed. If this function fails, the wrapped
 * stream is also destroyed.
 *
 * @param strm A pointer to another stream that should be wrapped.
 * @param comp_id An identifier describing the compressor to use.
 *
 * @return A pointer to an input stream on success, NULL on failure.
 */
SQFS_INTERNAL istream_t *istream_compressor_create(istream_t *strm,
						   int comp_id);

/**
 * @brief Create an output stream that transparently compresses data.
 *
 * @memberof ostream_t
 *
 * This function creates an output stream that transparently compresses all
 * data appended to it and writes the compressed data to an underlying, wrapped
 * output stream.
 *
 * The new stream takes ownership of the wrapped stream and destroys it when
 * the compressor stream is destroyed. If this function fails, the wrapped
 * stream is also destroyed.
 *
 * @param strm A pointer to another stream that should be wrapped.
 * @param comp_id An identifier describing the compressor to use.
 *
 * @return A pointer to an output stream on success, NULL on failure.
 */
SQFS_INTERNAL ostream_t *ostream_compressor_create(ostream_t *strm,
						   int comp_id);

/**
 * @brief Probe the buffered data in an istream to check if it is compressed.
 *
 * @memberof istream_t
 *
 * This function peeks into the internal buffer of an input stream to check
 * for magic signatures of various compressors.
 *
 * @param strm A pointer to an input stream to check
 * @param probe A callback used to check if raw/decoded data matches an
 *              expected format. Returns 0 if not, -1 on failure and +1
 *              on success.
 *
 * @return A compressor ID on success, 0 if no match was found, -1 on failure.
 */
SQFS_INTERNAL int istream_detect_compressor(istream_t *strm,
					    int (*probe)(const sqfs_u8 *data,
							 size_t size));

/**
 * @brief Resolve a compressor name to an ID.
 *
 * @param name A compressor name.
 *
 * @return A compressor ID on success, -1 on failure.
 */
SQFS_INTERNAL int io_compressor_id_from_name(const char *name);

/**
 * @brief Resolve a id to a  compressor name.
 *
 * @param id A compressor ID.
 *
 * @return A compressor name on success, NULL on failure.
 */
SQFS_INTERNAL const char *io_compressor_name_from_id(int id);

/**
 * @brief Check if support for a given compressor has been built in.
 *
 * @param id A compressor ID.
 *
 * @return True if the compressor is supported, false if not.
 */
SQFS_INTERNAL bool io_compressor_exists(int id);

#ifdef __cplusplus
}
#endif

#endif /* IO_XFRM_H */
