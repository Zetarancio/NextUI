#include "zlyme_artbook.h"

#include <stdio.h>
#include <string.h>

static int artbook_special(const char *name)
{
	return !strcmp(name, "Tools") ||
		!strcmp(name, "Recently Played") ||
		!strcmp(name, "Collections");
}

int zlyme_artbook_path(char *out, size_t out_len, const char *root, int list_mode, const char *name)
{
	int wrote;

	if (!out || out_len == 0 || !root || !root[0] || !name || !name[0])
		return 0;
	out[0] = '\0';
	if (strchr(name, '/') || strstr(name, ".."))
		return 0;
	if (artbook_special(name)) {
		if (list_mode)
			wrote = snprintf(out, out_len, "%s/list/%s-list.png", root, name);
		else
			wrote = snprintf(out, out_len, "%s/system/%s.png", root, name);
	} else if (list_mode) {
		wrote = snprintf(out, out_len, "%s/list/(%s)-list.png", root, name);
	} else {
		wrote = snprintf(out, out_len, "%s/system/(%s).png", root, name);
	}
	if (wrote < 0 || (size_t)wrote >= out_len) {
		out[0] = '\0';
		return 0;
	}
	return 1;
}
