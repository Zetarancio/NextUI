#include "launch_queue.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int fails;

static char *slurp(const char *path, size_t *n)
{
	FILE *f;
	char *buf;
	long sz;

	f = fopen(path, "rb");
	if (!f)
		return NULL;
	if (fseek(f, 0, SEEK_END) != 0) {
		fclose(f);
		return NULL;
	}
	sz = ftell(f);
	if (sz < 0) {
		fclose(f);
		return NULL;
	}
	if (fseek(f, 0, SEEK_SET) != 0) {
		fclose(f);
		return NULL;
	}
	buf = malloc((size_t)sz + 1);
	if (!buf) {
		fclose(f);
		return NULL;
	}
	if (sz && fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
		free(buf);
		fclose(f);
		return NULL;
	}
	fclose(f);
	buf[sz] = '\0';
	*n = (size_t)sz;
	return buf;
}

static void fail(const char *name)
{
	fprintf(stderr, "FAIL %s\n", name);
	fails++;
}

static void expect_bytes(const char *dir, const char *name, const char *exe,
			 const char *rom, const char *want)
{
	char dest[512];
	size_t n = 0;
	char *got;

	snprintf(dest, sizeof(dest), "%s/%s", dir, name);
	if (launch_queue_write_file(dest, exe, rom) != 0) {
		fail(name);
		return;
	}
	got = slurp(dest, &n);
	if (!got || n != strlen(want) || memcmp(got, want, n) != 0)
		fail(name);
	free(got);
}

static void expect_reject(const char *dir, const char *name,
			  const char *exe, const char *rom)
{
	char dest[512];
	FILE *f;
	size_t n = 0;
	char *got;

	snprintf(dest, sizeof(dest), "%s/%s", dir, name);
	f = fopen(dest, "w");
	if (!f) {
		fail(name);
		return;
	}
	fputs("SENTINEL\n", f);
	fclose(f);
	if (launch_queue_write_file(dest, exe, rom) == 0) {
		fail(name);
		return;
	}
	got = slurp(dest, &n);
	if (!got || strcmp(got, "SENTINEL\n") != 0)
		fail(name);
	free(got);
}

static void fill_exe(char *dst, size_t n)
{
	memset(dst, 'a', n);
	dst[0] = '/';
	memcpy(dst + n - 10, "/launch.sh", 10);
	dst[n] = '\0';
}

int main(int argc, char **argv)
{
	char tmpl[] = "/tmp/zlyme-lq-XXXXXX";
	char *dir;
	char spicy[128];
	char long_ok[4096];
	char long_bad[4098];
	const char *exe = "/storage/Emus/my355/GB.pak/launch.sh";
	const char *tool = "/storage/Tools/my355/Files.pak/launch.sh";

	if (argc >= 4 && strcmp(argv[1], "write") == 0) {
		const char *rom = argc >= 5 ? argv[4] : NULL;

		return launch_queue_write_file(argv[2], argv[3], rom) == 0 ? 0 : 1;
	}

	dir = mkdtemp(tmpl);
	if (!dir) {
		perror("mkdtemp");
		return 1;
	}

	expect_bytes(dir, "rom", exe, "/storage/Roms/GB/game.gb",
		     "ZLYME1\n2\n/storage/Emus/my355/GB.pak/launch.sh\n"
		     "/storage/Roms/GB/game.gb\n");
	expect_bytes(dir, "tool", tool, NULL,
		     "ZLYME1\n1\n/storage/Tools/my355/Files.pak/launch.sh\n");
	expect_bytes(dir, "empty-rom", tool, "",
		     "ZLYME1\n1\n/storage/Tools/my355/Files.pak/launch.sh\n");
	expect_bytes(dir, "hyphen", exe, "-leading.gb",
		     "ZLYME1\n2\n/storage/Emus/my355/GB.pak/launch.sh\n"
		     "-leading.gb\n");
	expect_bytes(dir, "tab", exe, "/r/a\tb",
		     "ZLYME1\n2\n/storage/Emus/my355/GB.pak/launch.sh\n"
		     "/r/a\tb\n");

	snprintf(spicy, sizeof(spicy),
		 "/r/it's \"q\" $x `y` (z); \xE3\x83\x86");
	{
		char dest[512];
		char expect[256];
		size_t n = 0;
		char *got;

		snprintf(dest, sizeof(dest), "%s/spicy", dir);
		if (launch_queue_write_file(dest, exe, spicy) != 0)
			fail("spicy");
		snprintf(expect, sizeof(expect), "ZLYME1\n2\n%s\n%s\n", exe, spicy);
		got = slurp(dest, &n);
		if (!got || n != strlen(expect) || memcmp(got, expect, n) != 0)
			fail("spicy-bytes");
		free(got);
	}

	fill_exe(long_ok, 4095);
	if (strlen(long_ok) != 4095)
		fail("long-setup");
	{
		char dest[512];

		snprintf(dest, sizeof(dest), "%s/long", dir);
		if (launch_queue_write_file(dest, long_ok, NULL) != 0)
			fail("long-ok");
	}
	fill_exe(long_bad, 4096);
	expect_reject(dir, "long-bad", long_bad, NULL);

	expect_reject(dir, "nl-rom", exe, "/r/a\nb");
	expect_reject(dir, "cr-rom", exe, "/r/a\rb");
	expect_reject(dir, "nl-exe", "/a/launch\n.sh", NULL);
	expect_reject(dir, "rel", "Emus/GB.pak/launch.sh", NULL);
	expect_reject(dir, "sh", "/bin/sh", NULL);
	expect_reject(dir, "empty", "", NULL);
	expect_reject(dir, "null", NULL, NULL);
	expect_reject(dir, "ctl", exe, "/r/\001");

	if (fails) {
		fprintf(stderr, "%d launch_queue failure%s\n", fails,
			fails == 1 ? "" : "s");
		return 1;
	}
	printf("launch_queue ok\n");
	return 0;
}
