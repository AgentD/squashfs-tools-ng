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
	size_t i;
	int ret;

	ret = parse_int(line, strlen(line), &i, 0, 0, priority);
	if (ret == SQFS_ERROR_CORRUPTED)
		goto fail_number;
	if (ret != 0)
		goto fail_ov;

	if (!isspace(line[i]))
		goto fail_filename;

	while (isspace(line[i]))
		++i;

	if (line[i] == '\0')
		goto fail_filename;

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
	split_line_t *sep;
	const char *end;

	*do_glob = false;
	*path_glob = false;
	*flags = 0;

	if (*(line++) != '[')
		return 0;

	end = strchr(line, ']');
	if (end == NULL) {
		fprintf(stderr, "%s: " PRI_SZ ": Missing `]`.\n",
			filename, line_no);
		return -1;
	}

	switch (split_line(line, end - line, ",", &sep)) {
	case SPLIT_LINE_OK:
		break;
	case SPLIT_LINE_ALLOC:
		fputs("out-of-memory.\n", stderr);
		return -1;
	default:
		fprintf(stderr, "%s: " PRI_SZ ": Malformed flag list.\n",
			filename, line_no);
		return -1;
	}

	++end;
	if (!isspace(*end)) {
		fprintf(stderr, "%s: " PRI_SZ ": Expected `<space> <filename>` "
			"after flag list.\n", filename, line_no);
		free(sep);
		return -1;
	}

	while (isspace(*end))
		++end;

	for (size_t i = 0; i < sep->count; ++i) {
		trim(sep->args[i]);

		if (strcmp(sep->args[i], "glob_no_path") == 0) {
			*do_glob = true;
			*path_glob = false;
		} else if (strcmp(sep->args[i], "glob") == 0) {
			*do_glob = true;
			*path_glob = true;
		} else if (strcmp(sep->args[i], "dont_fragment") == 0) {
			(*flags) |= SQFS_BLK_DONT_FRAGMENT;
		} else if (strcmp(sep->args[i], "dont_compress") == 0) {
			(*flags) |= SQFS_BLK_DONT_COMPRESS;
		} else if (strcmp(sep->args[i], "dont_deduplicate") == 0) {
			(*flags) |= SQFS_BLK_DONT_DEDUPLICATE;
		} else if (strcmp(sep->args[i], "nosparse") == 0) {
			(*flags) |= SQFS_BLK_IGNORE_SPARSE;
		} else {
			fprintf(stderr, "%s: " PRI_SZ ": Unknown flag `%s`.\n",
				filename, line_no, sep->args[i]);
			free(sep);
			return -1;
		}
	}

	free(sep);
	memmove(start, end, strlen(end) + 1);
	return 0;
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

int fstree_sort_files(fstree_t *fs, sqfs_istream_t *sortfile)
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
