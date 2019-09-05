/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * predef.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef SQFS_PREDEF_H
#define SQFS_PREDEF_H

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

#endif /* SQFS_PREDEF_H */
