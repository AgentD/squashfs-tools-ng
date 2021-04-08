#include "sqfs/meta_writer.h"
#include "sqfs/dir_writer.h"
#include "sqfs/compressor.h"
#include "sqfs/id_table.h"
#include "sqfs/inode.h"
#include "sqfs/super.h"
#include "sqfs/io.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

const char *README =
"This SuqashFS image contains ITSELF 42 times. Do NOT try to recursively\n"
"scan or unpack it. You will end up with an infinite amount of data!\n";

static void add_padding(sqfs_file_t *file)
{
	sqfs_u64 diff, size;

	size = file->get_size(file);
	diff = size % 4096;

	if (diff > 0)
		file->truncate(file, size + (4096 - diff));
}

static sqfs_inode_generic_t *create_file_inode(sqfs_id_table_t *idtbl,
					       unsigned int inode_num)
{
	sqfs_inode_generic_t *inode;

	inode = calloc(1, sizeof(*inode) + sizeof(sqfs_u32));
	inode->base.type = SQFS_INODE_FILE;
	inode->base.mode = SQFS_INODE_MODE_REG | 0644;
	inode->base.inode_number = inode_num;

	sqfs_id_table_id_to_index(idtbl, 0, &inode->base.uid_idx);
	sqfs_id_table_id_to_index(idtbl, 0, &inode->base.gid_idx);

	inode->data.file.file_size = 4096;
	inode->data.file.fragment_index = 0xFFFFFFFF;

	inode->payload_bytes_used = sizeof(sqfs_u32);
	inode->payload_bytes_available = sizeof(sqfs_u32);
	inode->extra[0] = (1 << 24) | inode->data.file.file_size;
	return inode;
}

int main(void)
{
	sqfs_meta_writer_t *inode_m, *dir_m;
	int ret, status = EXIT_FAILURE;
	sqfs_compressor_config_t cfg;
	sqfs_inode_generic_t *inode;
	unsigned int i, inode_num;
	sqfs_dir_writer_t *dirwr;
	sqfs_compressor_t *cmp;
	sqfs_id_table_t *idtbl;
	sqfs_u64 block_start;
	sqfs_super_t super;
	sqfs_file_t *file;
	sqfs_u32 offset;
	char buffer[32];

	/* get a file object referring to our destination file */
	file = sqfs_open_file("42.sqfs", SQFS_FILE_OPEN_OVERWRITE);
	if (file == NULL) {
		fputs("Error opening output file.\n", stderr);
		return EXIT_FAILURE;
	}

	/* initialize the super block with sane values */
	sqfs_super_init(&super, 4096, 0, SQFS_COMP_GZIP);

	if (sqfs_super_write(&super, file)) {
		fputs("Error witing super block.\n", stderr);
		goto out_file;
	}

	/* write the file data for the readme */
	if (file->write_at(file, sizeof(super), README, strlen(README))) {
		fputs("Error writing file data!\n", stderr);
		goto out_file;
	}

	/* create a compressor */
	sqfs_compressor_config_init(&cfg, super.compression_id,
				    super.block_size, 0);

	ret = sqfs_compressor_create(&cfg, &cmp);
	if (ret != 0) {
		fprintf(stderr, "Error creating compressor: %d.\n", ret);
		goto out_file;
	}

	/* create meta data writers for inodes and directories */
	inode_m = sqfs_meta_writer_create(file, cmp, 0);
	if (inode_m == NULL) {
		fputs("Error creating inode meta data writer.\n", stderr);
		goto out_cmp;
	}

	dir_m = sqfs_meta_writer_create(file, cmp,
					SQFS_META_WRITER_KEEP_IN_MEMORY);
	if (dir_m == NULL) {
		fputs("Error creating directory meta data writer.\n", stderr);
		goto out_im;
	}

	/* create a higher level directory writer on top of the meta writer */
	dirwr = sqfs_dir_writer_create(dir_m, 0);
	if (dirwr == NULL) {
		fputs("Error creating directory writer.\n", stderr);
		goto out_dm;
	}

	/* create an ID table */
	idtbl = sqfs_id_table_create(0);
	if (idtbl == NULL) {
		fputs("Error creating ID table.\n", stderr);
		goto out_dirwr;
	}

	/* generate inodes and directories */
	super.inode_table_start = file->get_size(file);
	inode_num = 1;

	sqfs_dir_writer_begin(dirwr, 0);

	for (i = 0; i < 42; ++i) {
		sprintf(buffer, "%02u.sqfs", i + 1);

		inode = create_file_inode(idtbl, inode_num++);
		sqfs_meta_writer_get_position(inode_m, &block_start, &offset);
		sqfs_meta_writer_write_inode(inode_m, inode);
		sqfs_dir_writer_add_entry(dirwr, buffer,
					  inode->base.inode_number,
					  (block_start << 16) | offset,
					  inode->base.mode);
		free(inode);
	}

	inode = create_file_inode(idtbl, inode_num++);
	inode->data.file.blocks_start = sizeof(super);
	inode->data.file.file_size = strlen(README);
	inode->extra[0] = (1 << 24) | inode->data.file.file_size;

	sqfs_meta_writer_get_position(inode_m, &block_start, &offset);
	sqfs_meta_writer_write_inode(inode_m, inode);
	sqfs_dir_writer_add_entry(dirwr, "README.txt",
				  inode->base.inode_number,
				  (block_start << 16) | offset,
				  inode->base.mode);
	free(inode);

	sqfs_dir_writer_end(dirwr);

	/* create an inode for the root directory */
	inode = sqfs_dir_writer_create_inode(dirwr, 0, 0xFFFFFFFF, 0);

	inode->base.mode = SQFS_INODE_MODE_DIR | 0755;
	inode->base.inode_number = inode_num++;
	sqfs_id_table_id_to_index(idtbl, 0, &inode->base.uid_idx);
	sqfs_id_table_id_to_index(idtbl, 0, &inode->base.gid_idx);

	sqfs_meta_writer_get_position(inode_m, &block_start, &offset);
	super.root_inode_ref = (block_start << 16) | offset;
	sqfs_meta_writer_write_inode(inode_m, inode);
	sqfs_free(inode);

	/* flush the meta data to the file */
	sqfs_meta_writer_flush(inode_m);
	sqfs_meta_writer_flush(dir_m);

	super.directory_table_start = file->get_size(file);
	sqfs_meta_write_write_to_file(dir_m);

	sqfs_id_table_write(idtbl, file, &super, cmp);

	super.inode_count = inode_num - 2;
	super.bytes_used = 4096;

	if (sqfs_super_write(&super, file)) {
		fputs("Error update the final super block.\n", stderr);
		goto out_file;
	}

	add_padding(file);

	/* cleanup */
	status = EXIT_SUCCESS;
	sqfs_destroy(idtbl);
out_dirwr:
	sqfs_destroy(dirwr);
out_dm:
	sqfs_destroy(dir_m);
out_im:
	sqfs_destroy(inode_m);
out_cmp:
	sqfs_destroy(cmp);
out_file:
	sqfs_destroy(file);
	return status;
}
