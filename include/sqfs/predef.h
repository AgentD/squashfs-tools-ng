/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * predef.h - This file is part of libsquashfs
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
#ifndef SQFS_PREDEF_H
#define SQFS_PREDEF_H

/**
 * @file predef.h
 *
 * @brief Includes forward declarations of data structures,
 *        macros and integer types.
 */

#include <sys/types.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32) || defined(__CYGWIN__)
	#ifdef SQFS_BUILDING_DLL
		#if defined(__GNUC__) || defined(__clang__)
			#define SQFS_API __attribute__ ((dllexport))
		#else
			#define SQFS_API __declspec(dllexport)
		#endif
	#else
		#if defined(__GNUC__) || defined(__clang__)
			#define SQFS_API __attribute__ ((dllimport))
		#else
			#define SQFS_API __declspec(dllimport)
		#endif
	#endif

	#define SQFS_INTERNAL
#else
	#if defined(__GNUC__) || defined(__clang__)
		#define SQFS_API __attribute__ ((visibility ("default")))
		#define SQFS_INTERNAL  __attribute__ ((visibility ("hidden")))
	#else
		#define SQFS_API
		#define SQFS_INTERNAL
	#endif
#endif

typedef struct sqfs_block_t sqfs_block_t;
typedef struct sqfs_data_writer_t sqfs_data_writer_t;
typedef struct sqfs_compressor_config_t sqfs_compressor_config_t;
typedef struct sqfs_compressor_t sqfs_compressor_t;
typedef struct sqfs_dir_writer_t sqfs_dir_writer_t;
typedef struct sqfs_dir_reader_t sqfs_dir_reader_t;
typedef struct sqfs_id_table_t sqfs_id_table_t;
typedef struct sqfs_meta_reader_t sqfs_meta_reader_t;
typedef struct sqfs_meta_writer_t sqfs_meta_writer_t;
typedef struct sqfs_xattr_reader_t sqfs_xattr_reader_t;
typedef struct sqfs_file_t sqfs_file_t;
typedef struct sqfs_sparse_map_t sqfs_sparse_map_t;
typedef struct sqfs_tree_node_t sqfs_tree_node_t;
typedef struct sqfs_data_reader_t sqfs_data_reader_t;
typedef struct sqfs_block_hooks_t sqfs_block_hooks_t;

typedef struct sqfs_fragment_t sqfs_fragment_t;
typedef struct sqfs_dir_header_t sqfs_dir_header_t;
typedef struct sqfs_dir_entry_t sqfs_dir_entry_t;
typedef struct sqfs_dir_index_t sqfs_dir_index_t;
typedef struct sqfs_inode_t sqfs_inode_t;
typedef struct sqfs_inode_dev_t sqfs_inode_dev_t;
typedef struct sqfs_inode_dev_ext_t sqfs_inode_dev_ext_t;
typedef struct sqfs_inode_ipc_t sqfs_inode_ipc_t;
typedef struct sqfs_inode_ipc_ext_t sqfs_inode_ipc_ext_t;
typedef struct sqfs_inode_slink_t sqfs_inode_slink_t;
typedef struct sqfs_inode_slink_ext_t sqfs_inode_slink_ext_t;
typedef struct sqfs_inode_file_t sqfs_inode_file_t;
typedef struct sqfs_inode_file_ext_t sqfs_inode_file_ext_t;
typedef struct sqfs_inode_dir_t sqfs_inode_dir_t;
typedef struct sqfs_inode_dir_ext_t sqfs_inode_dir_ext_t;
typedef struct sqfs_inode_generic_t sqfs_inode_generic_t;
typedef struct sqfs_super_t sqfs_super_t;
typedef struct sqfs_xattr_entry_t sqfs_xattr_entry_t;
typedef struct sqfs_xattr_value_t sqfs_xattr_value_t;
typedef struct sqfs_xattr_id_t sqfs_xattr_id_t;
typedef struct sqfs_xattr_id_table_t sqfs_xattr_id_table_t;

#endif /* SQFS_PREDEF_H */
