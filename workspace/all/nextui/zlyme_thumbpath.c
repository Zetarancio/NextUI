#include "zlyme_thumbpath.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

int zlyme_game_art_path(char *out, size_t out_len, const char *romdir, const char *name)
{
	static const char *suffixes[] = {".png", ".jpg", ".jpeg"};
	size_t i;

	if (!out || out_len == 0 || !romdir || !romdir[0] || !name || !name[0])
		return 0;
	out[0] = '\0';
	for (i = 0; i < sizeof(suffixes) / sizeof(suffixes[0]); i++) {
		int wrote = snprintf(out, out_len, "%s/.media/%s%s", romdir, name, suffixes[i]);
		if (wrote < 0 || (size_t)wrote >= out_len) {
			out[0] = '\0';
			return 0;
		}
		if (access(out, F_OK) == 0)
			return 1;
	}
	{
		int wrote = snprintf(out, out_len, "%s/.media/%s.png", romdir, name);
		if (wrote < 0 || (size_t)wrote >= out_len) {
			out[0] = '\0';
			return 0;
		}
	}
	return 1;
}
