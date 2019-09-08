/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * predef.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef SQFS_PREDEF_H
#define SQFS_PREDEF_H

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
typedef struct sqfs_block_processor_t sqfs_block_processor_t;
typedef struct sqfs_compressor_config_t sqfs_compressor_config_t;
typedef struct sqfs_compressor_t sqfs_compressor_t;
typedef struct sqfs_dir_writer_t sqfs_dir_writer_t;
typedef struct sqfs_id_table_t sqfs_id_table_t;
typedef struct sqfs_meta_reader_t sqfs_meta_reader_t;
typedef struct sqfs_meta_writer_t sqfs_meta_writer_t;
typedef struct sqfs_xattr_reader_t sqfs_xattr_reader_t;
typedef struct sqfs_file_t sqfs_file_t;

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

#endif /* SQFS_PREDEF_H */
