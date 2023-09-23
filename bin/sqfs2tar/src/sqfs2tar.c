/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * sqfs2tar.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "sqfs2tar.h"

static sqfs_ostream_t *out_file = NULL;

static int terminate_archive(void)
{
	char buffer[1024];

	memset(buffer, '\0', sizeof(buffer));

	return out_file->append(out_file, buffer, sizeof(buffer));
}

static int write_file_data(sqfs_dir_iterator_t *it, const sqfs_dir_entry_t *ent)
{
	sqfs_istream_t *in;
	int ret;

	ret = it->open_file_ro(it, &in);
	if (ret)
		return ret;

	do {
		ret = sqfs_istream_splice(in, out_file,
					  SQFS_DEFAULT_BLOCK_SIZE);
	} while (ret > 0);

	in = sqfs_drop(in);

	if (ret == 0)
		ret = padd_file(out_file, ent->size);

	return ret;
}

static int write_entry(sqfs_dir_iterator_t *it, const sqfs_dir_entry_t *ent)
{
	static unsigned int record_counter;
	sqfs_xattr_t *xattr = NULL;
	char *target = NULL;
	int ret;

	if (S_ISLNK(ent->mode) ||
	    (ent->flags & SQFS_DIR_ENTRY_FLAG_HARD_LINK)) {
		ret = it->read_link(it, &target);

		if (ret != 0) {
			sqfs_perror(ent->name, "reading link target", ret);
			return ret;
		}
	}

	ret = it->read_xattr(it, &xattr);
	if (ret != 0) {
		sqfs_perror(ent->name, "reading xattr data", ret);
		sqfs_free(target);
		return ret;
	}

	ret = write_tar_header(out_file, ent, target, xattr, record_counter++);
	if (ret)
		sqfs_perror(ent->name, "writing tar header", ret);

	if (S_ISREG(ent->mode) && ret == 0)
		ret = write_file_data(it, ent);

	sqfs_xattr_list_free(xattr);
	sqfs_free(target);
	return ret;
}

int main(int argc, char **argv)
{
	int ret, status = EXIT_FAILURE;
	sqfs_dir_iterator_t *it = NULL;

	process_args(argc, argv);

	ret = ostream_open_stdout(&out_file);
	if (ret) {
		sqfs_perror("stdout", "creating stream wrapper", ret);
		goto out;
	}

	if (compressor > 0) {
		xfrm_stream_t *xfrm = compressor_stream_create(compressor,NULL);
		sqfs_ostream_t *strm;

		if (xfrm == NULL)
			goto out;

		strm = ostream_xfrm_create(out_file, xfrm);
		sqfs_drop(out_file);
		sqfs_drop(xfrm);
		out_file = strm;

		if (out_file == NULL)
			goto out;
	}

	it = tar_compat_iterator_create(filename);
	if (it == NULL)
		goto out;

	if (!no_links) {
		sqfs_dir_iterator_t *hl;

		ret = sqfs_hard_link_filter_create(&hl, it);
		it = sqfs_drop(it);

		if (ret != 0) {
			sqfs_perror(filename, "creating hard link filter", ret);
			goto out;
		}

		it = hl;
	}

	for (;;) {
		sqfs_dir_entry_t *ent;

		ret = it->next(it, &ent);
		if (ret > 0)
			break;
		if (ret < 0) {
			sqfs_perror(filename, "reading directory entry", ret);
			goto out;
		}

		ret = write_entry(it, ent);
		if (ret == SQFS_ERROR_UNSUPPORTED) {
			fprintf(stderr, "WARNING: %s: unsupported file type\n",
				ent->name);
			if (dont_skip) {
				fputs("Not allowed to skip files, aborting!\n",
				      stderr);
				sqfs_free(ent);
				goto out;
			}
			fprintf(stderr, "Skipping %s\n", ent->name);
			sqfs_free(ent);
			continue;
		}

		if (ret) {
			sqfs_perror(ent->name, NULL, ret);
			sqfs_free(ent);
			goto out;
		}

		sqfs_free(ent);
	}

	if (terminate_archive())
		goto out;

	ret = out_file->flush(out_file);
	if (ret) {
		sqfs_perror(out_file->get_filename(out_file), NULL, ret);
		goto out;
	}

	status = EXIT_SUCCESS;
out:
	sqfs_drop(it);
	sqfs_drop(out_file);
	strlist_cleanup(&subdirs);
	free(root_becomes);
	return status;
}
