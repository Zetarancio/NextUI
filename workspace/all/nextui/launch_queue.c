#include "launch_queue.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define LAUNCH_MAX 4095

static int text_ok(const char *s, int exe)
{
	size_t i, n;

	if (!s || !s[0])
		return 0;
	n = strlen(s);
	if (n > LAUNCH_MAX)
		return 0;
	for (i = 0; i < n; i++) {
		unsigned char c = (unsigned char)s[i];

		if (c < 0x20 && c != '\t')
			return 0;
		if (c == 0x7f)
			return 0;
	}
	if (!exe)
		return 1;
	if (s[0] != '/')
		return 0;
	if (n < 10 || strcmp(s + n - 10, "/launch.sh") != 0)
		return 0;
	return 1;
}

static int write_line(FILE *f, const char *s)
{
	size_t n = strlen(s);

	if (fwrite(s, 1, n, f) != n)
		return -1;
	if (fputc('\n', f) == EOF)
		return -1;
	return 0;
}

int launch_queue_write_file(const char *dest, const char *exe, const char *rom)
{
	char tmpl[512];
	int fd, argc, n;
	FILE *f;

	if (!dest || !dest[0] || !text_ok(exe, 1))
		return -1;
	argc = 1;
	if (rom && rom[0]) {
		if (!text_ok(rom, 0))
			return -1;
		argc = 2;
	}
	n = snprintf(tmpl, sizeof(tmpl), "%s.XXXXXX", dest);
	if (n < 0 || (size_t)n >= sizeof(tmpl))
		return -1;
	fd = mkstemp(tmpl);
	if (fd < 0)
		return -1;
	f = fdopen(fd, "w");
	if (!f) {
		close(fd);
		unlink(tmpl);
		return -1;
	}
	if (fprintf(f, "ZLYME1\n%d\n", argc) < 0 || write_line(f, exe) != 0) {
		fclose(f);
		unlink(tmpl);
		return -1;
	}
	if (argc == 2 && write_line(f, rom) != 0) {
		fclose(f);
		unlink(tmpl);
		return -1;
	}
	if (fflush(f) != 0 || fsync(fileno(f)) != 0) {
		fclose(f);
		unlink(tmpl);
		return -1;
	}
	if (fclose(f) != 0) {
		unlink(tmpl);
		return -1;
	}
	if (rename(tmpl, dest) != 0) {
		unlink(tmpl);
		return -1;
	}
	return 0;
}

int launch_queue_write(const char *exe, const char *rom)
{
	return launch_queue_write_file("/tmp/next", exe, rom);
}
