/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * io.h - This file is part of libsquashfs
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
#ifndef SQFS_IO_H
#define SQFS_IO_H

#include "sqfs/predef.h"

/**
 * @file io.h
 *
 * @brief Contains low-level interfaces for abstracting file I/O
 *
 * The @ref sqfs_file_t interface abstracts I/O on a random-acces sread/write
 * file, @ref sqfs_ostream_t represents a buffered, sequential, append only
 * data stream, @ref sqfs_istream_t represents a buffered, sequential, read only
 * data stream.
 */

/**
 * @typedef sqfs_file_handle_t
 *
 * @brief Native handle type for file I/O
 */

#if defined(_WIN32) || defined(__WINDOWS__)
#include <handleapi.h>

typedef HANDLE sqfs_file_handle_t;
#else
typedef int sqfs_file_handle_t;
#endif

/**
 * @enum SQFS_FILE_OPEN_FLAGS
 *
 * @brief Flags for @ref sqfs_open_file
 */
typedef enum {
	/**
	 * @brief If set, access the file for reading only
	 *
	 * If not set, the file is expected to have a zero size after opening
	 * which can be grown with successive writes to end of the file.
	 *
	 * Opening an existing file with this flag cleared results in failure,
	 * unless the @ref SQFS_FILE_OPEN_OVERWRITE flag is also set.
	 */
	SQFS_FILE_OPEN_READ_ONLY = 0x01,

	/**
	 * @brief If the read only flag is not set, overwrite any
	 *        existing file.
	 *
	 * If the file alrady exists, it is truncated to zero bytes size and
	 * overwritten.
	 */
	SQFS_FILE_OPEN_OVERWRITE = 0x02,

	/**
	 * @brief If set, do not try to apply any character set transformations
	 *        to the file path.
	 *
	 * This flag currently only affects the Windows implementation. On
	 * Unix-like systems, the path is always passed to the OS API as-is
	 * and this flag has no effect.
	 *
	 * On Windows, the file paths are converted from UTF-8 to UTF-16 and
	 * then passed on to the wide-char API. If this flag is set, the path
	 * is used as-is and passed on to the 8-bit ANSI API instead, letting
	 * the OS decide how to convert and interpret the path.
	 */
	SQFS_FILE_OPEN_NO_CHARSET_XFRM = 0x04,

	/**
	 * @brief Do not use sparse file I/O APIs, always fill in zero bytes
	 *
	 * This flag currently has no effect on @ref sqfs_open_file, it changes
	 * the behavior of @ref sqfs_ostream_open_file and
	 * @ref sqfs_ostream_open_handle.
	 */
	SQFS_FILE_OPEN_NO_SPARSE = 0x08,

	SQFS_FILE_OPEN_ALL_FLAGS = 0x0F,
} SQFS_FILE_OPEN_FLAGS;

/**
 * @enum SQFS_FILE_SEEK_FLAGS
 *
 * @brief Controls the behavior of @ref sqfs_seek_native_file
 */
typedef enum {
	SQFS_FILE_SEEK_CURRENT = 0x00,
	SQFS_FILE_SEEK_START = 0x01,
	SQFS_FILE_SEEK_END = 0x02,
	SQFS_FILE_SEEK_TRUNCATE = 0x10,

	SQFS_FILE_SEEK_TYPE_MASK = 0x03,
	SQFS_FILE_SEEK_FLAG_MASK = 0x10,
} SQFS_FILE_SEEK_FLAGS;

/**
 * @interface sqfs_file_t
 *
 * @extends sqfs_object_t
 *
 * @brief Abstracts file I/O to make it easy to embedd SquashFS.
 *
 * Files are only copyable if they are read only, i.e. if a file has been
 * opened with write access, @ref sqfs_copy will always return NULL. The
 * other data types inside libsquashfs assume this to hold for all
 * implementations of this interface.
 */
struct sqfs_file_t {
	sqfs_object_t base;

	/**
	 * @brief Read a chunk of data from an absolute position.
	 *
	 * @param file A pointer to the file object.
	 * @param offset An absolute offset to read data from.
	 * @param buffer A pointer to a buffer to copy the data to.
	 * @param size The number of bytes to read from the file.
	 *
	 * @return Zero on success, an @ref SQFS_ERROR identifier on failure
	 *         that the data structures in libsquashfs that use this return
	 *         directly to the caller.
	 */
	int (*read_at)(sqfs_file_t *file, sqfs_u64 offset,
		       void *buffer, size_t size);

	/**
	 * @brief Write a chunk of data at an absolute position.
	 *
	 * @param file A pointer to the file object.
	 * @param offset An absolute offset to write data to.
	 * @param buffer A pointer to a buffer to write to the file.
	 * @param size The number of bytes to write from the buffer.
	 *
	 * @return Zero on success, an @ref SQFS_ERROR identifier on failure
	 *         that the data structures in libsquashfs that use this return
	 *         directly to the caller.
	 */
	int (*write_at)(sqfs_file_t *file, sqfs_u64 offset,
			const void *buffer, size_t size);

	/**
	 * @brief Get the number of bytes currently stored in the file.
	 *
	 * @param file A pointer to the file object.
	 */
	sqfs_u64 (*get_size)(const sqfs_file_t *file);

	/**
	 * @brief Extend or shrink a file to a specified size.
	 *
	 * @param file A pointer to the file object.
	 * @param size The new capacity of the file in bytes.
	 *
	 * @return Zero on success, an @ref SQFS_ERROR identifier on failure
	 *         that the data structures in libsquashfs that use this return
	 *         directly to the caller.
	 */
	int (*truncate)(sqfs_file_t *file, sqfs_u64 size);

	/**
	 * @brief Get the original name of the file used for opening it.
	 *
	 * @param file A pointer to the file object.
	 *
	 * @return A pointer to a string representing the file name.
	 */
	const char *(*get_filename)(sqfs_file_t *file);
};

/**
 * @interface sqfs_istream_t
 *
 * @extends sqfs_object_t
 *
 * @brief A sequential, read-only data stream.
 */
struct sqfs_istream_t {
	sqfs_object_t base;

	/**
	 * @brief Peek into the data buffered in an istream
	 *
	 * If the internal buffer is empty, the function tries to fetch more,
	 * which can block. It returns a positive return code if there is no
	 * more data to be read, a negative error code if reading failed. Since
	 * this and other functions can alter the buffer pointer and contents,
	 * do not store the pointers returned here across function calls.
	 *
	 * Higher level functions like @ref sqfs_istream_read (providing a
	 * Unix read() style API) are built on top of this primitive.
	 *
	 * @param strm A pointer to an sqfs_istream_t implementation.
	 * @param out Returns a pointer into an internal buffer on success.
	 * @param size Returns the number of bytes available in the buffer.
	 * @param want A number of bytes that the reader would like to have.
	 *             If there is less than this available, the implementation
	 *             can choose to do a blocking precache.
	 *
	 * @return Zero on success, a negative error code on failure,
	 *         a postive number on EOF.
	 */
	int (*get_buffered_data)(sqfs_istream_t *strm, const sqfs_u8 **out,
				 size_t *size, size_t want);

	/**
	 * @brief Mark a section of the internal buffer of an istream as used
	 *
	 * This marks the first `count` bytes of the internal buffer as used,
	 * forcing get_buffered_data to return data afterwards and potentially
	 * try to load more data.
	 *
	 * @param strm A pointer to an sqfs_istream_t implementation.
	 * @param count The number of bytes used up.
	 */
	void (*advance_buffer)(sqfs_istream_t *strm, size_t count);

	/**
	 * @brief Get the underlying filename of an input stream.
	 *
	 * @param strm The input stream to get the filename from.
	 *
	 * @return A string holding the underlying filename.
	 */
	const char *(*get_filename)(sqfs_istream_t *strm);
};

/**
 * @interface sqfs_ostream_t
 *
 * @extends sqfs_object_t
 *
 * @brief An append-only data stream.
 */
struct sqfs_ostream_t {
	sqfs_object_t base;

	/**
	 * @brief Append a block of data to an output stream.
	 *
	 * @param strm A pointer to an output stream.
	 * @param data A pointer to the data block to append. If NULL,
	 *             synthesize a chunk of zero bytes.
	 * @param size The number of bytes to append.
	 *
	 * @return Zero on success, -1 on failure.
	 */
	int (*append)(sqfs_ostream_t *strm, const void *data, size_t size);

	/**
	 * @brief Process all pending, buffered data and flush it to disk.
	 *
	 * If the stream performs some kind of transformation (e.g. transparent
	 * data compression), flushing caues the wrapped format to insert a
	 * termination token. Only call this function when you are absolutely
	 * DONE appending data, shortly before destroying the stream.
	 *
	 * @param strm A pointer to an output stream.
	 *
	 * @return Zero on success, -1 on failure.
	 */
	int (*flush)(sqfs_ostream_t *strm);

	/**
	 * @brief Get the underlying filename of a output stream.
	 *
	 * @param strm The output stream to get the filename from.
	 *
	 * @return A string holding the underlying filename.
	 */
	const char *(*get_filename)(sqfs_ostream_t *strm);
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Open a native file handle
 *
 * On Unix-like systems, this generates a file descriptor that needs to be
 * closed with close(). If opening fails, errno is preseved.
 *
 * On Windows, a HANDLE is created that needs to be disposed of
 * using CloseHandle(). If opening fails, GetLastError() is preseved.
 * If @ref SQFS_FILE_OPEN_NO_CHARSET_XFRM is set, the given string is passed
 * to the ANSI API that interprets the string according to the the currently
 * set codepage. If the flag is not present, the string is assumed to be UTF-8,
 * the function internally converts it to UTF-16 and uses the wide char API.
 *
 * @param out Returns a native file handle on success
 * @param filename The path to the file to open
 * @param flags A set of @ref SQFS_FILE_OPEN_FLAGS controlling how the
 *              file is opened.
 *
 * @return Zero on success, a negative @ref SQFS_ERROR code on failure.
 *         If an unknown flag was used, @ref SQFS_ERROR_UNSUPPORTED is returned.
 */
SQFS_API int sqfs_open_native_file(sqfs_file_handle_t *out,
				   const char *filename, sqfs_u32 flags);

/**
 * @brief Despose of a file handle returned by @ref sqfs_open_native_file
 *
 * @param fd A native OS file handle
 */
SQFS_API void sqfs_close_native_file(sqfs_file_handle_t fd);

/**
 * @brief Duplicate a file handle returned by @ref sqfs_open_native_file
 *
 * @param in A native OS file handle
 * @param out A new file handle pointing to the same kernel object
 *
 * @return Zero on success, a negative @ref SQFS_ERROR code on failure.
 */
SQFS_API int sqfs_duplicate_native_file(sqfs_file_handle_t in,
					sqfs_file_handle_t *out);

/**
 * @brief Set the file read/write pointer on a native file handle
 *
 * @param in A native OS file handle
 * @param offset A relative offset to seek to
 * @param flags A combination of @ref SQFS_FILE_SEEK_FLAGS flags
 *
 * @return Zero on success, a negative @ref SQFS_ERROR code on failure.
 */
SQFS_API int sqfs_seek_native_file(sqfs_file_handle_t hnd,
				   sqfs_s64 offset, sqfs_u32 flags);

/**
 * @brief Open a file through the operating systems filesystem API
 *
 * This function internally creates an instance of a reference implementation
 * of the @ref sqfs_file_t interface that uses the operating systems native
 * API for file I/O.
 *
 * On Unix-like systems, if the open call fails, this function makes sure to
 * preserves the value in errno indicating the underlying problem. Similarly,
 * on Windows, the implementation tries to preserve the GetLastError value.
 *
 * @param filename The name of the file to open.
 * @param flags A set of @ref SQFS_FILE_OPEN_FLAGS.
 *
 * @return A pointer to a file object on success, NULL on allocation failure,
 *         failure to open the file or if an unknown flag was set.
 */
SQFS_API sqfs_file_t *sqfs_open_file(const char *filename, sqfs_u32 flags);

/**
 * @brief Create an input stream for an OS native file handle.
 *
 * @memberof sqfs_istream_t
 *
 * The functions takes up ownership of the file handle and takes care
 * of cleaning it up. On failure, the handle remains usable, and ownership
 * remains with the caller.
 *
 * @param out Returns a pointer to an input stream on success.
 * @param path The name to associate with the handle.
 * @param fd A native file handle.
 * @param flags A combination of @ref SQFS_FILE_OPEN_FLAGS flags.
 *
 * @return Zero on success, a negative @ref SQFS_ERROR number on failure
 */
SQFS_API
int sqfs_istream_open_handle(sqfs_istream_t **out, const char *path,
			     sqfs_file_handle_t fd, sqfs_u32 flags);

/**
 * @brief Create an output stream that writes to an OS native file handle.
 *
 * @memberof sqfs_ostream_t
 *
 * If the flag SQFS_FILE_OPEN_NO_SPARSE is set, the underlying implementation
 * always writes chunks of zero bytes when passing a NULL pointer to append.
 * Otherwise, it tries to use seek/truncate style APIs to create sparse output
 * files.
 *
 * @param out Returns a pointer to an output stream on success.
 * @param path The name to associate with the handle.
 * @param fd A native file handle.
 * @param flags A combination of @ref SQFS_FILE_OPEN_FLAGS flags.
 *
 * @return Zero on success, a negative @ref SQFS_ERROR number on failure
 */
SQFS_API int sqfs_ostream_open_handle(sqfs_ostream_t **out, const char *path,
				      sqfs_file_handle_t hnd, sqfs_u32 flags);

/**
 * @brief Create an input stream that reads from a file.
 *
 * @memberof sqfs_istream_t
 *
 * The flag @ref SQFS_FILE_OPEN_READ_ONLY is implicitly assumed to be set,
 * since the function constructs a read-only primitive. If either the flags
 * @ref SQFS_FILE_OPEN_OVERWRITE or @ref SQFS_FILE_OPEN_NO_SPARSE are set,
 * an error is returend.
 *
 * @param out Returns a pointer to an input stream on success.
 * @param path A path to the file to open or create.
 * @param flags A combination of @ref SQFS_FILE_OPEN_FLAGS flags.
 *
 * @return Zero on success, a negative @ref SQFS_ERROR number on failure
 */
SQFS_API int sqfs_istream_open_file(sqfs_istream_t **out, const char *path,
				    sqfs_u32 flags);

/**
 * @brief Create an output stream that writes to a file.
 *
 * @memberof sqfs_ostream_t
 *
 * If the file does not yet exist, it is created. If it does exist this
 * function fails, unless the flag SQFS_FILE_OPEN_OVERWRITE is set, in which
 * case the file is opened and its contents are discarded.
 *
 * If the flag SQFS_FILE_OPEN_NO_SPARSE is set, the underlying implementation
 * always writes chunks of zero bytes when passing a NULL pointer to append.
 * Otherwise, it tries to use seek/truncate style APIs to create sparse output
 * files.
 *
 * @param out Returns a pointer to an output stream on success.
 * @param path A path to the file to open or create.
 * @param flags A combination of @ref SQFS_FILE_OPEN_FLAGS flags.
 *
 * @return Zero on success, a negative @ref SQFS_ERROR number on failure
 */
SQFS_API int sqfs_ostream_open_file(sqfs_ostream_t **out,
				    const char *path, sqfs_u32 flags);

/**
 * @brief Read data from an input stream
 *
 * @memberof sqfs_istream_t
 *
 * This function implements a Unix-style read() function on top of
 * an @ref sqfs_istream_t, taking care of buffer management internally.
 *
 * @param strm A pointer to an input stream.
 * @param data A buffer to read into.
 * @param size The number of bytes to read into the buffer.
 *
 * @return The number of bytes actually read on success, a
 *         negative @ref SQFS_ERROR code on failure, 0 on end-of-file.
 */
SQFS_API sqfs_s32 sqfs_istream_read(sqfs_istream_t *strm,
				    void *data, size_t size);

/**
 * @brief Skip over a number of bytes in an input stream.
 *
 * @memberof sqfs_istream_t
 *
 * @param strm A pointer to an input stream.
 * @param size The number of bytes to seek forward.
 *
 * @return Zero on success, a negative @ref SQFS_ERROR code on failure.
 */
SQFS_API int sqfs_istream_skip(sqfs_istream_t *strm, sqfs_u64 size);

/**
 * @brief Dump data from an input stream to an output stream
 *
 * @memberof sqfs_istream_t
 *
 * @param in A pointer to an input stream to read from.
 * @param out A pointer to an output stream to append to.
 * @param size The number of bytes to copy over.
 *
 * @return Zero on success, a negative @ref SQFS_ERROR code on failure.
 */
SQFS_API sqfs_s32 sqfs_istream_splice(sqfs_istream_t *in, sqfs_ostream_t *out,
				      sqfs_u32 size);

#ifdef __cplusplus
}
#endif

#endif /* SQFS_IO_H */
