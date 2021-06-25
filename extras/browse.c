#include "sqfs/data_reader.h"
#include "sqfs/compressor.h"
#include "sqfs/dir_reader.h"
#include "sqfs/id_table.h"
#include "sqfs/inode.h"
#include "sqfs/super.h"
#include "sqfs/error.h"
#include "sqfs/block.h"
#include "sqfs/dir.h"
#include "sqfs/io.h"

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>

#include <readline/readline.h>
#include <readline/history.h>

static sqfs_dir_reader_t *dr;
static sqfs_super_t super;
static sqfs_inode_generic_t *working_dir;
static sqfs_id_table_t *idtbl;
static sqfs_data_reader_t *data;

/*****************************************************************************/

static void change_directory(const char *dirname)
{
	sqfs_inode_generic_t *inode;
	int ret;

	if (dirname == NULL || *dirname == '/') {
		free(working_dir);
		working_dir = NULL;

		sqfs_dir_reader_get_root_inode(dr, &working_dir);
	}

	if (dirname != NULL) {
		ret = sqfs_dir_reader_find_by_path(dr, working_dir,
						   dirname, &inode);
		if (ret != 0) {
			printf("Error resolving '%s', error code %d\n",
			       dirname, ret);
			return;
		}

		free(working_dir);
		working_dir = inode;
	}
}

/*****************************************************************************/

static void list_directory(const char *dirname)
{
	sqfs_inode_generic_t *root, *inode;
	size_t i, max_len, len, col_count;
	sqfs_dir_entry_t *ent;
	int ret;

	/* Get the directory inode we want to dump and open the directory */
	if (dirname == NULL) {
		ret = sqfs_dir_reader_open_dir(dr, working_dir, 0);
		if (ret)
			goto fail_open;
	} else if (*dirname == '/') {
		sqfs_dir_reader_get_root_inode(dr, &root);

		ret = sqfs_dir_reader_find_by_path(dr, root, dirname, &inode);
		sqfs_free(root);
		if (ret)
			goto fail_resolve;

		ret = sqfs_dir_reader_open_dir(dr, inode, 0);
		sqfs_free(inode);
		if (ret)
			goto fail_open;
	} else {
		ret = sqfs_dir_reader_find_by_path(dr, working_dir,
						   dirname, &inode);
		if (ret)
			goto fail_resolve;

		ret = sqfs_dir_reader_open_dir(dr, inode, 0);
		sqfs_free(inode);
		if (ret)
			goto fail_open;
	}

	/* first pass over the directory to figure out column count/length */
	for (max_len = 0; ; max_len = len > max_len ? len : max_len) {
		ret = sqfs_dir_reader_read(dr, &ent);
		if (ret > 0)
			break;

		if (ret < 0) {
			fputs("Error while reading directory list\n", stderr);
			break;
		}

		len = ent->size + 1;
		sqfs_free(ent);
	}

	col_count = 79 / (max_len + 1);
	col_count = col_count < 1 ? 1 : col_count;
	i = 0;

	/* second pass for printing directory contents */
	sqfs_dir_reader_rewind(dr);

	for (;;) {
		ret = sqfs_dir_reader_read(dr, &ent);
		if (ret > 0)
			break;

		if (ret < 0) {
			fputs("Error while reading directory list\n", stderr);
			break;
		}

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
fail_open:
	printf("Error opening '%s', error code %d\n", dirname, ret);
	return;
fail_resolve:
	printf("Error resolving '%s', error code %d\n", dirname, ret);
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
	sqfs_inode_generic_t *inode, *root;
	sqfs_dir_index_t *idx;
	sqfs_u32 uid, gid;
	const char *type;
	char buffer[64];
	time_t timeval;
	struct tm *tm;
	size_t i;
	int ret;

	/* get the inode we are interested in */
	if (filename == NULL) {
		printf("Missing argument: file name\n");
		return;
	}

	if (*filename == '/') {
		sqfs_dir_reader_get_root_inode(dr, &root);
		ret = sqfs_dir_reader_find_by_path(dr, root, filename, &inode);
		sqfs_free(root);
		if (ret)
			goto fail_resolve;
	} else {
		ret = sqfs_dir_reader_find_by_path(dr, working_dir,
						   filename, &inode);
		if (ret)
			goto fail_resolve;
	}

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
		printf("Blocks start: %lu\n",
		       inode->data.file_ext.blocks_start);
		printf("Block count: %lu\n",
		       (unsigned long)sqfs_inode_get_file_block_count(inode));
		printf("Fragment index: 0x%X\n",
		       inode->data.file_ext.fragment_idx);
		printf("Fragment offset: %u\n",
		       inode->data.file_ext.fragment_offset);
		printf("File size: %lu\n", inode->data.file_ext.file_size);
		printf("Sparse: %lu\n", inode->data.file_ext.sparse);
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
	sqfs_inode_generic_t *inode, *root;
	sqfs_u64 offset = 0;
	char buffer[512];
	sqfs_s32 diff;
	int ret;

	/* get the inode of the file */
	if (filename == NULL) {
		printf("Missing argument: file name\n");
		return;
	}

	if (*filename == '/') {
		sqfs_dir_reader_get_root_inode(dr, &root);
		ret = sqfs_dir_reader_find_by_path(dr, root, filename, &inode);
		sqfs_free(root);
	} else {
		ret = sqfs_dir_reader_find_by_path(dr, working_dir,
						   filename, &inode);
	}

	if (ret) {
		printf("Error resolving '%s', error code %d\n", filename, ret);
		return;
	}

	/* Use the high level read-like function that uses the data-reader
	 * internal cache. There are also other functions for direct block
	 * or fragment access.
	 */
	for (;;) {
		diff = sqfs_data_reader_read(data, inode, offset, buffer,
					     sizeof(buffer));
		if (diff == 0)
			break;
		if (diff < 0) {
			printf("Error reading from file '%s', error code %d\n",
			       filename, diff);
			break;
		}

		fwrite(buffer, 1, diff, stdout);
		offset += diff;
	}

	sqfs_free(inode);
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

	file = sqfs_open_file(argv[1], SQFS_FILE_OPEN_READ_ONLY);
	if (file == NULL) {
		perror(argv[1]);
		return EXIT_FAILURE;
	}

	/* read the super block, create a compressor and
	   process the compressor options */
	if (sqfs_super_read(&super, file)) {
		fprintf(stderr, "%s: error reading super block.\n", argv[1]);
		goto out_fd;
	}

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
	dr = sqfs_dir_reader_create(&super, cmp, file, 0);
	if (dr == NULL) {
		fprintf(stderr, "%s: error creating directory reader.\n",
			argv[1]);
		goto out_id;
	}

	if (sqfs_dir_reader_get_root_inode(dr, &working_dir)) {
		fprintf(stderr, "%s: error reading root inode.\n", argv[1]);
		goto out_dir;
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
	for (;;) {
		free(buffer);
		buffer = readline("$ ");

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
	sqfs_destroy(data);
out_dir:
	if (working_dir != NULL)
		free(working_dir);
	sqfs_destroy(dr);
out_id:
	sqfs_destroy(idtbl);
out_cmp:
	sqfs_destroy(cmp);
out_fd:
	sqfs_destroy(file);
	return status;
}
