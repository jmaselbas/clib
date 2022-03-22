/* SPDX-License-Identifier: ISC */
/* SPDX-FileCopyrightText: 2022 Jules Maselbas */
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <limits.h>

#include "arg.h"

static void
die(const char *fmt, ...)
{
	va_list va;

	va_start(va, fmt);
	vfprintf(stderr, fmt, va);
	va_end(va);

	exit(1);
}

static char *
fgetsz(char *s, int size, FILE *stream)
{
	int len, c;

	if (size == 0)
		return NULL;
	size--;
	for (len = 0; len < size; len++) {
		c = fgetc(stream);
		if (c == EOF || c == '\n' || c == '\0')
			break;
		s[len] = c;
	}
	s[len] = '\0';

	return s;
}

static int
fexist(const char *name)
{
	FILE *f = fopen(name, "r");
	if (f)
		fclose(f);
	return f != NULL;
}

struct clib_entry {
	const char *name;
	uint32_t offset;
	uint32_t size;
};

struct clib {
	const char *name;
	uint32_t nr_entry;
	struct clib_entry *entries;
};

struct clib clib;

static void
parse(FILE *f)
{
	char magic[] = "CLIB\x1a\x1e";
	char unknow[9];
	char buf[PATH_MAX];
	size_t i;

	if (fread(&magic, strlen("CLIB\x1a\x1e"), 1, f) != 1)
		die("invalid format: not enough data\n");
	if (strcmp(magic, "CLIB\x1a\x1e") != 0)
		die("invalid format: magic does not match\n");
	if (fread(&unknow, sizeof(unknow), 1, f) != 1)
		die("invalid format: not enough data\n");

	fgetsz(buf, sizeof(buf), f);
	clib.name = strdup(buf);

	if (fread(&clib.nr_entry, sizeof(clib.nr_entry), 1, f) != 1)
		die("invalid format: not enough data\n");

	clib.entries = malloc(clib.nr_entry * sizeof(struct clib_entry));
	if (!clib.entries)
		die("malloc: %s\n", strerror(errno));

	for (i = 0; i < clib.nr_entry; i++) {
		uint32_t offset, size;
		uint32_t pad;

		fgetsz(buf, sizeof(buf), f);
		pad = fgetc(f); /* always 0 ? */
		fread(&offset, sizeof(offset), 1, f);
		fread(&pad, sizeof(pad), 1, f); /* always 0 ? */
		fread(&size, sizeof(size), 1, f);
		fread(&pad, sizeof(pad), 1, f); /* always 0 ? */

		clib.entries[i].name = strdup(buf);
		clib.entries[i].offset = offset;
		clib.entries[i].size = size;
	}
}

static void
do_list(void)
{
	struct clib_entry e;
	size_t i;

	for (i = 0; i < clib.nr_entry; i++) {
		e = clib.entries[i];
		printf("%s\n", e.name);
	}
}

static int
do_extract(FILE *f, int force)
{
	struct clib_entry e;
	char chunk[4096];
	size_t size, n, i;
	FILE *p;
	int ret = 0;

	for (i = 0; i < clib.nr_entry; i++) {
		e = clib.entries[i];

		if (!force && fexist(e.name)) {
			fprintf(stderr, "%s: File exists\n", e.name);
			ret = 1;
			continue;
		}
		p = fopen(e.name, "w");
		if (!p) {
			fprintf(stderr, "%s: %s\n", e.name, strerror(errno));
			ret = 1;
			continue;
		}
		fseek(f, e.offset, SEEK_SET);
		size = e.size;
		while (size > 0) {
			if (size > sizeof(chunk))
				n = sizeof(chunk);
			else
				n = size;
			fread(chunk, n, 1, f);
			fwrite(chunk, n, 1, p);
			size -= n;
		}
		fclose(p);
	}

	return ret;
}

enum {
	NONE,
	TEST,
	LIST,
	EXTR,
};

char *argv0;

static void
usage(void)
{
	printf("usage: %s [-tlfxv] FILE\n", argv0);
	printf("option:\n");
	printf("  -t      test if FILE is reconized as CLIB\n");
	printf("  -l      list files\n");
	printf("  -f      force overwrite of output file\n");
	printf("  -x      extract files\n");
	printf("  -v      display version\n");
	exit(1);
}

int
main(int argc, char **argv)
{
	char *infile;
	FILE *f;
	int act = NONE;
	int ret = 0;
	int force = 0;

	ARGBEGIN {
	case 'f':
		force = 1;
		break;
	case 't':
		act = TEST;
		break;
	case 'l':
		act = LIST;
		break;
	case 'x':
		act = EXTR;
		break;
	case 'v':
		printf("%s %s", argv0, "0.0.0");
		exit(0);
		break;
	case 'h':
	default:
		usage();
	} ARGEND;

	if (argc < 1) {
		printf("missing file argument\n");
		usage();
	}
	if (act == NONE)
		usage();

	infile = argv[0];
	f = fopen(infile, "r");
	if (!f)
		die("%s: %s\n", infile, strerror(errno));

	parse(f);

	switch (act) {
	case LIST:
		do_list();
		break;
	case EXTR:
		ret = do_extract(f, force);
		break;
	case NONE:
		break;
	}

	fclose(f);

	return ret;
}
