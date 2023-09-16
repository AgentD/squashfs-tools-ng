/* SPDX-License-Identifier: 0BSD */
/*
 * browse.c
 *
 * Copyright (C) 2020 David Oberhollenzer <goliath@infraroot.at>
 */
#include "sqfs/data_reader.h"
#include "sqfs/compressor.h"
#include "sqfs/dir_reader.h"
#include "sqfs/id_table.h"
#include "sqfs/inode.h"
#include "sqfs/block.h"
#include "sqfs/super.h"
#include "sqfs/error.h"
#include "sqfs/dir.h"
#include "sqfs/io.h"
#include "compat.h"

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>

#include <readline/readline.h>
#include <readline/history.h>

static sqfs_dir_reader_t *dr;
static sqfs_super_t super;
static sqfs_id_table_t *idtbl;
static sqfs_data_reader_t *data;
static sqfs_u64 working_dir;
static char *prompt;

static int resolve_ref(const char *path, sqfs_u64 *out)
{
	sqfs_dir_reader_state_t state;
	sqfs_inode_generic_t *inode;
	sqfs_dir_node_t *ent;
	const char *end;
	sqfs_u64 ref;
	size_t len;
	int ret;

	ref = *path == '/' ? super.root_inode_ref : working_dir;

	while (path != NULL && *path != '\0') {
		if (*path == '/') {
			while (*path == '/')
				++path;
			continue;
		}

		end = strchr(path, '/');
		len = (end == NULL) ? strlen(path) : (size_t)(end - path);

		ret = sqfs_dir_reader_get_inode(dr, ref, &inode);
		if (ret != 0)
			return ret;

		ret = sqfs_dir_reader_open_dir(dr, inode, &state, 0);
		sqfs_free(inode);

		if (ret != 0)
			return ret;

		do {
			ret = sqfs_dir_reader_read(dr, &state, &ent);
			if (ret < 0)
				return ret;
			if (ret > 0)
				return 1;

			ret = strncmp((const char *)ent->name, path, len);
			free(ent);
			if (ret == 0 && ent->name[len] != '\0')
				return 1;
		} while (ret != 0);

		ref = state.ent_ref;
		path = end;
	}

	*out = ref;
	return 0;
}

static char *add_prefix(const char *pfx, size_t pfxlen, char *path)
{
	size_t len;
	char *out;

	if (path == NULL) {
		out = calloc(1, pfxlen + 1);
		if (out == NULL)
			goto fail;
		memcpy(out, pfx, pfxlen);
	} else {
		len = strlen(path) + 1;
		out = realloc(path, pfxlen + 1 + len);
		if (out == NULL)
			goto fail;

		memmove(out + pfxlen + 1, out, len);
		memcpy(out, pfx, pfxlen);
		out[pfxlen] = '/';
	}

	return out;
fail:
	fputs("out-of-memory\n", stderr);
	free(path);
	return NULL;
}

static char *full_path(void)
{
	sqfs_inode_generic_t *inode = NULL, *pnode = NULL;
	sqfs_dir_node_t *ent = NULL;
	char *path = NULL;
	sqfs_u64 iref = working_dir;
	int ret;

	if (iref == super.root_inode_ref)
		return strdup("/");

	ret = sqfs_dir_reader_get_inode(dr, iref, &inode);
	if (ret) {
		fprintf(stderr, "Loading inode: %d\n", ret);
		return NULL;
	}

	while (iref != super.root_inode_ref) {
		sqfs_dir_reader_state_t state;
		sqfs_u32 parent;
		sqfs_u64 pref;

		if (inode->base.type == SQFS_INODE_DIR) {
			parent = inode->data.dir.parent_inode;
		} else if (inode->base.type == SQFS_INODE_EXT_DIR) {
			parent = inode->data.dir_ext.parent_inode;
		} else {
			fputs("Inode not a directory??\n", stderr);
			goto fail;
		}

		ret = sqfs_dir_reader_resolve_inum(dr, parent, &pref);
		if (ret) {
			fputs("Parent inode not cached!\n", stderr);
			goto fail;
		}

		ret = sqfs_dir_reader_get_inode(dr, pref, &pnode);
		if (ret) {
			fprintf(stderr, "Error loading parent inode: %d\n",
				ret);
			goto fail;
		}

		ret = sqfs_dir_reader_open_dir(dr, pnode, &state, 0);
		if (ret) {
			fprintf(stderr, "Error opening parent dir: %d\n",
				ret);
			goto fail;
		}

		do {
			sqfs_free(ent);
			ret = sqfs_dir_reader_read(dr, &state, &ent);
		} while (ret == 0 && state.ent_ref != iref);

		if (ret) {
			fprintf(stderr, "Error finding entry in dir: %d\n",
				ret);
			goto fail;
		}

		path = add_prefix((const char *)ent->name, ent->size + 1, path);
		if (path == NULL)
			goto fail;

		sqfs_free(ent);
		sqfs_free(inode);
		inode = pnode;
		pnode = NULL;
		ent = NULL;
		iref = pref;
	}

	path = add_prefix("", 0, path);
	if (path == NULL)
		goto fail;

	sqfs_free(inode);
	return path;
fail:
	sqfs_free(inode);
	sqfs_free(pnode);
	sqfs_free(ent);
	free(path);
	return NULL;
}

/*****************************************************************************/

static void change_directory(const char *dirname)
{
	char *path, *str;
	const char *end;
	size_t len;
	int ret;

	if (dirname == NULL) {
		working_dir = super.root_inode_ref;
	} else {
		ret = resolve_ref(dirname, &working_dir);
		if (ret < 0) {
			fprintf(stderr, "%s: error resolving path: %d\n",
				dirname, ret);
		} else if (ret > 0) {
			fprintf(stderr, "%s: no such file or directory\n",
				dirname);
		}
	}

	path = full_path();
	if (path == NULL)
		return;

	end = strrchr(path, '/');
	end = end == NULL ? path : (end + 1);
	len = strlen(end);

	str = calloc(1, len + 3);
	if (str == NULL) {
		free(path);
		return;
	}

	memcpy(str, end, len);
	str[len] = '$';
	str[len + 1] = ' ';

	free(prompt);
	free(path);
	prompt = str;
}

/*****************************************************************************/

static void list_directory(const char *dirname)
{
	sqfs_dir_reader_state_t state, init_state;
	size_t i, max_len, len, col_count;
	sqfs_inode_generic_t *inode;
	sqfs_dir_node_t *ent;
	sqfs_u64 ref;
	int ret;

	/* Get the directory inode we want to dump and open the directory */
	if (dirname == NULL) {
		ref = working_dir;
	} else {
		ret = resolve_ref(dirname, &ref);
		if (ret < 0)
			goto fail_resolve;
		if (ret > 0)
			goto fail_no_ent;
	}

	ret = sqfs_dir_reader_get_inode(dr, ref, &inode);
	if (ret)
		goto fail_open;

	ret = sqfs_dir_reader_open_dir(dr, inode, &state, 0);
	sqfs_free(inode);
	if (ret)
		goto fail_open;

	/* first pass over the directory to figure out column count/length */
	init_state = state;

	for (max_len = 0; ; max_len = len > max_len ? len : max_len) {
		ret = sqfs_dir_reader_read(dr, &state, &ent);
		if (ret > 0)
			break;
		if (ret < 0)
			goto fail_read;

		len = ent->size + 1;
		sqfs_free(ent);
	}

	col_count = 79 / (max_len + 1);
	col_count = col_count < 1 ? 1 : col_count;
	i = 0;

	state = init_state;

	/* second pass for printing directory contents */
	for (;;) {
		ret = sqfs_dir_reader_read(dr, &state, &ent);
		if (ret > 0)
			break;
		if (ret < 0)
			goto fail_read;

		/* entries always use basic types only */
		switch (ent->type) {
		case SQFS_INODE_DIR:
			fputs("\033[01;34m", stdout);
			break;
		case SQFS_INODE_FILE:
			break;
		case SQFS_INODE_SLINK:
			fputs("\033[01;36m", stdout);
			break;
		case SQFS_INODE_BDEV:
			fputs("\033[22;33m", stdout);
			break;
		case SQFS_INODE_CDEV:
			fputs("\033[01;33m", stdout);
			break;
		case SQFS_INODE_FIFO:
		case SQFS_INODE_SOCKET:
			fputs("\033[01;35m", stdout);
			break;
		default:
			break;
		}

		len = ent->size + 1;

		printf("%.*s", ent->size + 1, ent->name);
		fputs("\033[0m", stdout);
		sqfs_free(ent);

		++i;
		if (i == col_count) {
			i = 0;
			fputc('\n', stdout);
		} else {
			while (len++ < max_len)
				fputc(' ', stdout);
			fputc(' ', stdout);
		}
	}

	if (i != 0)
		fputc('\n', stdout);

	return;
fail_read:
	fputs("Error while reading directory list\n", stderr);
	return;
fail_open:
	printf("Error opening '%s', error code %d\n", dirname, ret);
	return;
fail_resolve:
	printf("Error resolving '%s', error code %d\n", dirname, ret);
	return;
fail_no_ent:
	fprintf(stderr, "%s: no such file or directory\n", dirname);
	return;
}

/*****************************************************************************/

static void mode_to_str(sqfs_u16 mode, char *p)
{
	*(p++) = (mode & SQFS_INODE_OWNER_R) ? 'r' : '-';
	*(p++) = (mode & SQFS_INODE_OWNER_W) ? 'w' : '-';

	switch (mode & (SQFS_INODE_OWNER_X | SQFS_INODE_SET_UID)) {
	case SQFS_INODE_OWNER_X | SQFS_INODE_SET_UID: *(p++) = 's'; break;
	case SQFS_INODE_OWNER_X:                      *(p++) = 'x'; break;
	case SQFS_INODE_SET_UID:                      *(p++) = 'S'; break;
	default:                                      *(p++) = '-'; break;
	}

	*(p++) = (mode & SQFS_INODE_GROUP_R) ? 'r' : '-';
	*(p++) = (mode & SQFS_INODE_GROUP_W) ? 'w' : '-';

	switch (mode & (SQFS_INODE_GROUP_X | SQFS_INODE_SET_GID)) {
	case SQFS_INODE_GROUP_X | SQFS_INODE_SET_GID: *(p++) = 's'; break;
	case SQFS_INODE_GROUP_X:                      *(p++) = 'x'; break;
	case SQFS_INODE_SET_GID:                      *(p++) = 'S'; break;
	default:                                      *(p++) = '-'; break;
	}

	*(p++) = (mode & SQFS_INODE_OTHERS_R) ? 'r' : '-';
	*(p++) = (mode & SQFS_INODE_OTHERS_W) ? 'w' : '-';

	switch (mode & (SQFS_INODE_OTHERS_X | SQFS_INODE_STICKY)) {
	case SQFS_INODE_OTHERS_X | SQFS_INODE_STICKY: *(p++) = 't'; break;
	case SQFS_INODE_OTHERS_X:                     *(p++) = 'x'; break;
	case SQFS_INODE_STICKY:                       *(p++) = 'T'; break;
	default:                                      *(p++) = '-'; break;
	}

	*p = '\0';
}

static void stat_cmd(const char *filename)
{
	sqfs_inode_generic_t *inode;
	sqfs_dir_index_t *idx;
	sqfs_u32 uid, gid;
	const char *type;
	char buffer[64];
	time_t timeval;
	struct tm *tm;
	sqfs_u64 ref;
	size_t i;
	int ret;

	/* get the inode we are interested in */
	if (filename == NULL) {
		printf("Missing argument: file name\n");
		return;
	}

	ret = resolve_ref(filename, &ref);
	if (ret)
		goto fail_resolve;

	ret = sqfs_dir_reader_get_inode(dr, ref, &inode);
	if (ret)
		goto fail_resolve;

	/* some basic information */
	switch (inode->base.type) {
	case SQFS_INODE_DIR:        type = "directory";                 break;
	case SQFS_INODE_FILE:       type = "file";                      break;
	case SQFS_INODE_SLINK:      type = "symbolic link";             break;
	case SQFS_INODE_BDEV:       type = "block device";              break;
	case SQFS_INODE_CDEV:       type = "character device";          break;
	case SQFS_INODE_FIFO:       type = "named pipe";                break;
	case SQFS_INODE_SOCKET:     type = "socket";                    break;
	case SQFS_INODE_EXT_DIR:    type = "extended directory";        break;
	case SQFS_INODE_EXT_FILE:   type = "extended file";             break;
	case SQFS_INODE_EXT_SLINK:  type = "extended symbolic link";    break;
	case SQFS_INODE_EXT_BDEV:   type = "extended block device";     break;
	case SQFS_INODE_EXT_CDEV:   type = "extended character device"; break;
	case SQFS_INODE_EXT_FIFO:   type = "extended named pipe";       break;
	case SQFS_INODE_EXT_SOCKET: type = "extended socket";           break;
	default:                    type = "UNKNOWN";                   break;
	}

	printf("Stat: %s\n", filename);
	printf("Type: %s\n", type);
	printf("Inode number: %u\n", inode->base.inode_number);

	mode_to_str(inode->base.mode & ~SQFS_INODE_MODE_MASK, buffer);
	printf("Access: 0%o/%s\n",
	       (unsigned int)inode->base.mode & ~SQFS_INODE_MODE_MASK,
	       buffer);

	/* resolve and print UID/GID */
	if (sqfs_id_table_index_to_id(idtbl, inode->base.uid_idx, &uid)) {
		strcpy(buffer, "-- error --");
	} else {
		sprintf(buffer, "%u", uid);
	}

	printf("UID: %s (index = %u)\n", buffer, inode->base.uid_idx);

	if (sqfs_id_table_index_to_id(idtbl, inode->base.gid_idx, &gid)) {
		strcpy(buffer, "-- error --");
	} else {
		sprintf(buffer, "%u", gid);
	}

	printf("GID: %s (index = %u)\n", buffer, inode->base.gid_idx);

	/* last modification time stamp */
	timeval = inode->base.mod_time;
	tm = gmtime(&timeval);
	strftime(buffer, sizeof(buffer), "%a, %d %b %Y %T %z", tm);
	printf("Last modified: %s (%u)\n", buffer, inode->base.mod_time);

	/* inode type specific data */
	switch (inode->base.type) {
	case SQFS_INODE_BDEV:
	case SQFS_INODE_CDEV:
		printf("Hard link count: %u\n", inode->data.dev.nlink);
		printf("Device number: %u\n", inode->data.dev.devno);
		break;
	case SQFS_INODE_EXT_BDEV:
	case SQFS_INODE_EXT_CDEV:
		printf("Hard link count: %u\n", inode->data.dev_ext.nlink);
		printf("Xattr index: 0x%X\n", inode->data.dev_ext.xattr_idx);
		printf("Device number: %u\n", inode->data.dev_ext.devno);
		break;
	case SQFS_INODE_FIFO:
	case SQFS_INODE_SOCKET:
		printf("Hard link count: %u\n", inode->data.ipc.nlink);
		break;
	case SQFS_INODE_EXT_FIFO:
	case SQFS_INODE_EXT_SOCKET:
		printf("Hard link count: %u\n", inode->data.ipc_ext.nlink);
		printf("Xattr index: 0x%X\n", inode->data.ipc_ext.xattr_idx);
		break;
	case SQFS_INODE_SLINK:
		printf("Hard link count: %u\n", inode->data.slink.nlink);
		printf("Link target: %.*s\n",
		       (int)inode->data.slink.target_size,
		       (const char *)inode->extra);
		break;
	case SQFS_INODE_EXT_SLINK:
		printf("Hard link count: %u\n", inode->data.slink_ext.nlink);
		printf("Xattr index: 0x%X\n", inode->data.slink_ext.xattr_idx);
		printf("Link target: %.*s\n",
		       (int)inode->data.slink_ext.target_size,
		       (const char *)inode->extra);
		break;
	case SQFS_INODE_FILE:
		printf("Blocks start: %u\n", inode->data.file.blocks_start);
		printf("Block count: %lu\n",
		       (unsigned long)sqfs_inode_get_file_block_count(inode));
		printf("Fragment index: 0x%X\n",
		       inode->data.file.fragment_index);
		printf("Fragment offset: %u\n",
		       inode->data.file.fragment_offset);
		printf("File size: %u\n", inode->data.file.file_size);

		for (i = 0; i < sqfs_inode_get_file_block_count(inode); ++i) {
			printf("\tBlock #%lu size: %u (%s)\n", (unsigned long)i,
			       SQFS_ON_DISK_BLOCK_SIZE(inode->extra[i]),
			       SQFS_IS_BLOCK_COMPRESSED(inode->extra[i]) ?
			       "compressed" : "uncompressed");
		}
		break;
	case SQFS_INODE_EXT_FILE:
		printf("Blocks start: " PRI_U64 "\n",
		       inode->data.file_ext.blocks_start);
		printf("Block count: %lu\n",
		       (unsigned long)sqfs_inode_get_file_block_count(inode));
		printf("Fragment index: 0x%X\n",
		       inode->data.file_ext.fragment_idx);
		printf("Fragment offset: %u\n",
		       inode->data.file_ext.fragment_offset);
		printf("File size: " PRI_U64 "\n",
		       inode->data.file_ext.file_size);
		printf("Sparse: " PRI_U64 "\n",
		       inode->data.file_ext.sparse);
		printf("Hard link count: %u\n", inode->data.file_ext.nlink);
		printf("Xattr index: 0x%X\n", inode->data.file_ext.xattr_idx);

		for (i = 0; i < sqfs_inode_get_file_block_count(inode); ++i) {
			printf("\tBlock #%lu size: %u (%s)\n", (unsigned long)i,
			       SQFS_ON_DISK_BLOCK_SIZE(inode->extra[i]),
			       SQFS_IS_BLOCK_COMPRESSED(inode->extra[i]) ?
			       "compressed" : "uncompressed");
		}
		break;
	case SQFS_INODE_DIR:
		printf("Start block: %u\n", inode->data.dir.start_block);
		printf("Offset: %u\n", inode->data.dir.offset);
		printf("Hard link count: %u\n", inode->data.dir.nlink);
		printf("Size: %u\n", inode->data.dir.size);
		printf("Parent inode: %u\n", inode->data.dir.parent_inode);
		break;
	case SQFS_INODE_EXT_DIR:
		printf("Start block: %u\n", inode->data.dir_ext.start_block);
		printf("Offset: %u\n", inode->data.dir_ext.offset);
		printf("Hard link count: %u\n", inode->data.dir_ext.nlink);
		printf("Size: %u\n", inode->data.dir_ext.size);
		printf("Parent inode: %u\n", inode->data.dir_ext.parent_inode);
		printf("Xattr index: 0x%X\n", inode->data.dir_ext.xattr_idx);
		printf("Directory index entries: %u\n",
		       inode->data.dir_ext.inodex_count);

		if (inode->data.dir_ext.size == 0)
			break;

		/* dump the extended directories fast-lookup index */
		for (i = 0; ; ++i) {
			ret = sqfs_inode_unpack_dir_index_entry(inode, &idx, i);
			if (ret == SQFS_ERROR_OUT_OF_BOUNDS)
				break;
			if (ret < 0) {
				printf("Error reading index, error code %d\n",
				       ret);
				break;
			}

			printf("\tIndex: %u\n", idx->index);
			printf("\tStart block: %u\n", idx->start_block);
			printf("\tSize: %u\n", idx->size + 1);
			printf("\tEntry: %.*s\n\n",
			       (int)(idx->size + 1), idx->name);

			sqfs_free(idx);
		}
		break;
	default:
		break;
	}

	sqfs_free(inode);
	return;
fail_resolve:
	printf("Error resolving '%s', error code %d\n", filename, ret);
	return;
}

/*****************************************************************************/

static void cat_cmd(const char *filename)
{
	sqfs_inode_generic_t *inode;
	sqfs_istream_t *stream;
	char buffer[512];
	sqfs_u64 ref;
	int ret;

	/* get the inode of the file */
	if (filename == NULL) {
		printf("Missing argument: file name\n");
		return;
	}

	ret = resolve_ref(filename, &ref);
	if (ret) {
		printf("Error resolving '%s', error code %d\n", filename, ret);
		return;
	}

	ret = sqfs_dir_reader_get_inode(dr, ref, &inode);
	if (ret) {
		printf("Error reading inode for '%s', error code %d\n",
		       filename, ret);
		return;
	}

	ret = sqfs_data_reader_create_stream(data, inode, filename, &stream);
	sqfs_free(inode);
	if (ret) {
		printf("Opening file '%s', error code %d\n", filename, ret);
		return;
	}

	/* Use the high level read-like function that uses the data-reader
	 * internal cache. There are also other functions for direct block
	 * or fragment access.
	 */
	for (;;) {
		ret = sqfs_istream_read(stream, buffer, sizeof(buffer));
		if (ret == 0)
			break;
		if (ret < 0) {
			printf("Error reading from file '%s', error code %d\n",
			       filename, ret);
			break;
		}

		fwrite(buffer, 1, ret, stdout);
	}

	sqfs_drop(stream);
}

/*****************************************************************************/

static void pwd_cmd(const char *filename)
{
	(void)filename;
	char *path;

	path = full_path();
	if (path != NULL) {
		printf("%s\n", path);
		free(path);
	}
}

/*****************************************************************************/

static const struct {
	const char *cmd;
	void (*handler)(const char *arg);
} commands[] = {
	{ "ls", list_directory },
	{ "cd", change_directory },
	{ "stat", stat_cmd },
	{ "cat", cat_cmd },
	{ "pwd", pwd_cmd },
};

int main(int argc, char **argv)
{
	char *cmd, *arg, *buffer = NULL;
	int ret, status = EXIT_FAILURE;
	sqfs_compressor_config_t cfg;
	sqfs_compressor_t *cmp;
	sqfs_file_t *file;
	size_t i;

	/* open the SquashFS file we want to read */
	if (argc != 2) {
		fputs("Usage: sqfsbrowse <squashfs-file>\n", stderr);
		return EXIT_FAILURE;
	}

	if (sqfs_file_open(&file, argv[1], SQFS_FILE_OPEN_READ_ONLY)) {
		fprintf(stderr, "%s: error opening file.\n", argv[1]);
		return EXIT_FAILURE;
	}

	/* read the super block, create a compressor and
	   process the compressor options */
	if (sqfs_super_read(&super, file)) {
		fprintf(stderr, "%s: error reading super block.\n", argv[1]);
		goto out_fd;
	}

	working_dir = super.root_inode_ref;

	sqfs_compressor_config_init(&cfg, super.compression_id,
				    super.block_size,
				    SQFS_COMP_FLAG_UNCOMPRESS);

	ret = sqfs_compressor_create(&cfg, &cmp);
	if (ret != 0) {
		fprintf(stderr, "%s: error creating compressor: %d.\n",
			argv[1], ret);
		goto out_fd;
	}

	/* Create and read the UID/GID mapping table */
	idtbl = sqfs_id_table_create(0);
	if (idtbl == NULL) {
		fputs("Error creating ID table.\n", stderr);
		goto out_cmp;
	}

	if (sqfs_id_table_read(idtbl, file, &super, cmp)) {
		fprintf(stderr, "%s: error loading ID table.\n", argv[1]);
		goto out_id;
	}

	/* create a directory reader and get the root inode */
	dr = sqfs_dir_reader_create(&super, cmp, file,
				    SQFS_DIR_READER_DOT_ENTRIES);
	if (dr == NULL) {
		fprintf(stderr, "%s: error creating directory reader.\n",
			argv[1]);
		goto out_id;
	}

	/* create a data reader */
	data = sqfs_data_reader_create(file, super.block_size, cmp, 0);

	if (data == NULL) {
		fprintf(stderr, "%s: error creating data reader.\n",
			argv[1]);
		goto out_dir;
	}

	if (sqfs_data_reader_load_fragment_table(data, &super)) {
		fprintf(stderr, "%s: error loading fragment table.\n",
			argv[1]);
		goto out;
	}

	/* main readline loop */
	prompt = strdup("$ ");
	if (prompt == NULL) {
		fputs("out-of-memory\n", stderr);
		goto out;
	}

	for (;;) {
		free(buffer);
		buffer = readline(prompt);

		if (buffer == NULL)
			goto out;

		for (cmd = buffer; isspace(*cmd); ++cmd)
			;

		if (*cmd == '\0')
			continue;

		add_history(cmd);

		for (arg = cmd; *arg != '\0' && !isspace(*arg); ++arg)
			;

		if (isspace(*arg)) {
			*(arg++) = '\0';
			while (isspace(*arg))
				++arg;
			if (*arg == '\0')
				arg = NULL;
		} else {
			arg = NULL;
		}

		for (i = 0; i < sizeof(commands) / sizeof(commands[0]); ++i) {
			if (strcmp(commands[i].cmd, cmd) == 0) {
				commands[i].handler(arg);
				break;
			}
		}
	}

	/* cleanup */
	status = EXIT_SUCCESS;
	free(buffer);
out:
	free(prompt);
	sqfs_drop(data);
out_dir:
	sqfs_drop(dr);
out_id:
	sqfs_drop(idtbl);
out_cmp:
	sqfs_drop(cmp);
out_fd:
	sqfs_drop(file);
	return status;
}
