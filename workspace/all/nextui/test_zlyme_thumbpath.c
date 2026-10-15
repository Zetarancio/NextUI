#define _GNU_SOURCE
#include "zlyme_thumbpath.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int failed;

static void expect(int cond, const char *what)
{
	if (!cond) {
		fprintf(stderr, "fail: %s\n", what);
		failed = 1;
	}
}

static void touch(const char *path)
{
	FILE *f = fopen(path, "w");
	if (!f) {
		perror(path);
		failed = 1;
		return;
	}
	fputs("x", f);
	fclose(f);
}

int main(void)
{
	char dir[] = "/tmp/zlyme-thumb-XXXXXX";
	char media[256];
	char out[512];
	char tiny[8];
	char *made;

	made = mkdtemp(dir);
	expect(made != NULL, "temp dir");
	if (!made)
		return 1;
	snprintf(media, sizeof(media), "%s/.media", dir);
	expect(mkdir(media, 0700) == 0, "media dir");

	expect(zlyme_game_art_path(out, sizeof(out), dir, "Game") == 1, "missing returns");
	expect(strstr(out, "/.media/Game.png") != NULL, "missing is png");
	expect(access(out, F_OK) != 0, "missing png is not created");

	snprintf(out, sizeof(out), "%s/other.png", media);
	touch(out);
	expect(zlyme_game_art_path(out, sizeof(out), dir, "Game") == 1, "unrelated still returns");
	expect(strstr(out, "other.png") == NULL, "unrelated image was chosen");
	expect(strstr(out, "/.media/Game.png") != NULL, "unrelated fell through to png");

	snprintf(out, sizeof(out), "%s/Game.jpeg", media);
	touch(out);
	expect(zlyme_game_art_path(out, sizeof(out), dir, "Game") == 1, "jpeg only");
	expect(strstr(out, "/.media/Game.jpeg") != NULL, "jpeg suffix");

	snprintf(out, sizeof(out), "%s/Game.jpg", media);
	touch(out);
	expect(zlyme_game_art_path(out, sizeof(out), dir, "Game") == 1, "jpg over jpeg");
	expect(strstr(out, "/.media/Game.jpg") != NULL, "jpg wins over jpeg");
	expect(strstr(out, ".jpeg") == NULL, "jpeg was chosen beside jpg");

	snprintf(out, sizeof(out), "%s/Game.png", media);
	touch(out);
	expect(zlyme_game_art_path(out, sizeof(out), dir, "Game") == 1, "png over jpg");
	expect(strstr(out, "/.media/Game.png") != NULL, "png wins");

	snprintf(out, sizeof(out), "%s/Bad.jpg", media);
	{
		FILE *f = fopen(out, "w");
		expect(f != NULL, "corrupt file");
		if (f) {
			fputs("not a jpeg", f);
			fclose(f);
		}
	}
	expect(zlyme_game_art_path(out, sizeof(out), dir, "Bad") == 1, "corrupt path");
	expect(strstr(out, "/.media/Bad.jpg") != NULL, "corrupt jpg is still the path");

	expect(zlyme_game_art_path(tiny, sizeof(tiny), dir, "Game") == 0, "short buffer");
	expect(tiny[0] == '\0', "short buffer cleared");
	expect(zlyme_game_art_path(out, sizeof(out), dir, "") == 0, "empty name");

	printf("thumb path ok\n");
	return failed ? 1 : 0;
}
