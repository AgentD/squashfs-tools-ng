/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * sort_by_file.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "mkfs.h"

static int decode_priority(const char *filename, size_t line_no,
			   char *line, sqfs_s64 *priority)
{
	bool negative = false;
	size_t i = 0;

	if (line[0] == '-') {
		negative = true;
		i = 1;
	}

	if (!isdigit(line[i]))
		goto fail_number;

	*priority = 0;

	for (; isdigit(line[i]); ++i) {
		sqfs_s64 x = line[i] - '0';

		if ((*priority) >= ((0x7FFFFFFFFFFFFFFFL - x) / 10L))
			goto fail_ov;

		(*priority) = (*priority) * 10 + x;
	}

	if (!isspace(line[i]))
		goto fail_filename;

	while (isspace(line[i]))
		++i;

	if (line[i] == '\0')
		goto fail_filename;

	if (negative)
		(*priority) = -(*priority);

	memmove(line, line + i, strlen(line + i) + 1);
	return 0;
fail_number:
	fprintf(stderr, "%s: " PRI_SZ ": Line must start with "
		"numeric sort priority.\n",
		filename, line_no);
	return -1;
fail_ov:
	fprintf(stderr, "%s: " PRI_SZ ": Numeric overflow in sort priority.\n",
		filename, line_no);
	return -1;
fail_filename:
	fprintf(stderr, "%s: " PRI_SZ ": Expacted `<space> <filename>` "
		"after sort priority.\n",
		filename, line_no);
	return -1;
}

static int decode_filename(const char *filename, size_t line_no, char *buffer)
{
	char *src, *dst;

	if (buffer[0] == '"') {
		src = buffer + 1;
		dst = buffer;

		for (;;) {
			if (src[0] == '\0')
				goto fail_match;

			if (src[0] == '"') {
				++src;
				break;
			}

			if (src[0] == '\\') {
				switch (src[1]) {
				case '\\':
					*(dst++) = '\\';
					src += 2;
					break;
				case '"':
					*(dst++) = '"';
					src += 2;
					break;
				default:
					goto fail_escape;
				}
			} else {
				*(dst++) = *(src++);
			}
		}

		if (*src != '\0')
			return -1;
	}

	if (canonicalize_name(buffer))
		goto fail_canon;
	return 0;
fail_canon:
	fprintf(stderr, "%s: " PRI_SZ ": Malformed filename.\n",
		filename, line_no);
	return -1;
fail_escape:
	fprintf(stderr, "%s: " PRI_SZ ": Unknown escape sequence `\\%c` "
		"in filename.\n", filename, line_no, src[1]);
	return -1;
fail_match:
	fprintf(stderr, "%s: " PRI_SZ ": Unmatched '\"' in filename.\n",
		filename, line_no);
	return -1;
}

static int decode_flags(const char *filename, size_t line_no, bool *do_glob,
			bool *path_glob, int *flags, char *line)
{
	char *start = line;

	*do_glob = false;
	*path_glob = false;
	*flags = 0;

	if (*(line++) != '[')
		return 0;

	for (;;) {
		while (isspace(*line))
			++line;

		if (*line == ']') {
			++line;
			break;
		}

		if (strncmp(line, "glob_no_path", 12) == 0) {
			line += 12;
			*do_glob = true;
			*path_glob = false;
		} else if (strncmp(line, "glob", 4) == 0) {
			line += 4;
			*do_glob = true;
			*path_glob = true;
		} else if (strncmp(line, "dont_fragment", 13) == 0) {
			line += 13;
			(*flags) |= SQFS_BLK_DONT_FRAGMENT;
		} else if (strncmp(line, "dont_compress", 13) == 0) {
			line += 13;
			(*flags) |= SQFS_BLK_DONT_COMPRESS;
		} else if (strncmp(line, "dont_deduplicate", 16) == 0) {
			line += 16;
			(*flags) |= SQFS_BLK_DONT_DEDUPLICATE;
		} else if (strncmp(line, "nosparse", 8) == 0) {
			line += 8;
			(*flags) |= SQFS_BLK_IGNORE_SPARSE;
		} else {
			goto fail_flag;
		}

		while (isspace(*line))
			++line;

		if (*line == ']') {
			++line;
			break;
		}

		if (*(line++) != ',')
			goto fail_sep;
	}

	if (!isspace(*line))
		goto fail_fname;

	while (isspace(*line))
		++line;

	memmove(start, line, strlen(line) + 1);
	return 0;
fail_fname:
	fprintf(stderr, "%s: " PRI_SZ ": Expected `<space> <filename>` "
		"after flag list.\n", filename, line_no);
	return -1;
fail_sep:
	fprintf(stderr, "%s: " PRI_SZ ": Unexpected '%c' after flag.\n",
		filename, line_no, *line);
	return -1;
fail_flag:
	fprintf(stderr, "%s: " PRI_SZ ": Unknown flag `%.3s...`.\n",
		filename, line_no, line);
	return -1;
}

static void sort_file_list(fstree_t *fs)
{
	tree_node_t *out = NULL, *out_last = NULL;

	while (fs->files != NULL) {
		tree_node_t *low = fs->files, *low_prev = NULL;
		tree_node_t *it = low->next_by_type, *prev = low;

		while (it != NULL) {
			if (it->data.file.priority < low->data.file.priority) {
				low = it;
				low_prev = prev;
			}
			prev = it;
			it = it->next_by_type;
		}

		if (low_prev == NULL) {
			fs->files = low->next_by_type;
		} else {
			low_prev->next_by_type = low->next_by_type;
		}

		if (out == NULL) {
			out = low;
		} else {
			out_last->next_by_type = low;
		}

		out_last = low;
		low->next_by_type = NULL;
	}

	fs->files = out;
}

int fstree_sort_files(fstree_t *fs, istream_t *sortfile)
{
	const char *filename;
	size_t line_num = 1;
	tree_node_t *node;

	for (node = fs->files; node != NULL; node = node->next_by_type) {
		node->data.file.priority = 0;
		node->data.file.flags = 0;
		node->flags &= ~FLAG_FILE_ALREADY_MATCHED;
	}

	filename = sortfile->get_filename(sortfile);

	for (;;) {
		bool do_glob, path_glob, have_match;
		char *line = NULL;
		sqfs_s64 priority;
		int ret, flags;

		ret = istream_get_line(sortfile, &line, &line_num,
				       ISTREAM_LINE_LTRIM |
				       ISTREAM_LINE_RTRIM |
				       ISTREAM_LINE_SKIP_EMPTY);
		if (ret != 0) {
			free(line);
			if (ret < 0)
				return -1;
			break;
		}

		if (line[0] == '#') {
			free(line);
			continue;
		}

		if (decode_priority(filename, line_num, line, &priority)) {
			free(line);
			return -1;
		}

		if (decode_flags(filename, line_num, &do_glob, &path_glob,
				 &flags, line)) {
			free(line);
			return -1;
		}

		if (decode_filename(filename, line_num, line)) {
			free(line);
			return -1;
		}

		have_match = false;

		for (node = fs->files; node != NULL; node = node->next_by_type) {
			char *path;

			if (node->flags & FLAG_FILE_ALREADY_MATCHED)
				continue;

			path = fstree_get_path(node);
			if (path == NULL) {
				fprintf(stderr, "%s: " PRI_SZ ": out-of-memory\n",
					filename, line_num);
				free(line);
				return -1;
			}

			if (canonicalize_name(path)) {
				fprintf(stderr,
					"%s: " PRI_SZ ": [BUG] error "
					"reconstructing node path\n",
					filename, line_num);
				free(line);
				free(path);
				return -1;
			}

			if (do_glob) {
				ret = fnmatch(line, path,
					      path_glob ? FNM_PATHNAME : 0);
			} else {
				ret = strcmp(path, line);
			}

			free(path);

			if (ret == 0) {
				have_match = true;
				node->data.file.flags = flags;
				node->data.file.priority = priority;
				node->flags |= FLAG_FILE_ALREADY_MATCHED;

				if (!do_glob)
					break;
			}
		}

		if (!have_match) {
			fprintf(stderr, "WARNING: %s: " PRI_SZ ": no match "
				"for '%s'.\n",
				filename, line_num, line);
		}

		free(line);
	}

	sort_file_list(fs);
	return 0;
}
