#include "zlyme_artbook.h"

#include <stdio.h>
#include <string.h>

static int artbook_name_ok(const char *name)
{
	return name && name[0] && !strchr(name, '/') && !strstr(name, "..");
}

static int artbook_write(char *out, size_t out_len, const char *path)
{
	size_t n = strlen(path);

	if (n + 1 > out_len) {
		out[0] = '\0';
		return 0;
	}
	memcpy(out, path, n + 1);
	return 1;
}

int zlyme_artbook_system(char *out, size_t out_len, const char *root, const char *tag)
{
	char path[1024];
	int wrote;

	if (!out || out_len == 0 || !root || !root[0] || !artbook_name_ok(tag))
		return 0;
	if (strchr(tag, ' '))
		return 0;
	out[0] = '\0';
	wrote = snprintf(path, sizeof(path), "%s/system/(%s).png", root, tag);
	if (wrote < 0 || (size_t)wrote >= sizeof(path))
		return 0;
	return artbook_write(out, out_len, path);
}

int zlyme_artbook_tools_category(char *out, size_t out_len, const char *root)
{
	char path[1024];
	int wrote;

	if (!out || out_len == 0 || !root || !root[0])
		return 0;
	out[0] = '\0';
	wrote = snprintf(path, sizeof(path), "%s/special/tools.png", root);
	if (wrote < 0 || (size_t)wrote >= sizeof(path))
		return 0;
	return artbook_write(out, out_len, path);
}

int zlyme_artbook_tool(char *out, size_t out_len, const char *root, const char *pak)
{
	char path[1024];
	int wrote;

	if (!out || out_len == 0 || !root || !root[0] || !artbook_name_ok(pak))
		return 0;
	out[0] = '\0';
	wrote = snprintf(path, sizeof(path), "%s/tools/%s.png", root, pak);
	if (wrote < 0 || (size_t)wrote >= sizeof(path))
		return 0;
	return artbook_write(out, out_len, path);
}

int zlyme_artbook_recent(char *out, size_t out_len, const char *root)
{
	char path[1024];
	int wrote;

	if (!out || out_len == 0 || !root || !root[0])
		return 0;
	out[0] = '\0';
	wrote = snprintf(path, sizeof(path), "%s/special/recent.png", root);
	if (wrote < 0 || (size_t)wrote >= sizeof(path))
		return 0;
	return artbook_write(out, out_len, path);
}

int zlyme_recent_user_path(char *out, size_t out_len, const char *shared)
{
	char path[1024];
	int wrote;

	if (!out || out_len == 0 || !shared || !shared[0])
		return 0;
	out[0] = '\0';
	wrote = snprintf(path, sizeof(path), "%s/artbook/special/recent.png", shared);
	if (wrote < 0 || (size_t)wrote >= sizeof(path))
		return 0;
	return artbook_write(out, out_len, path);
}

int zlyme_recent_bg_choice(int legacy, int user, int global, int firmware, int *artbook)
{
	if (artbook)
		*artbook = 0;
	if (legacy)
		return 1;
	if (user)
		return 2;
	if (global)
		return 3;
	if (firmware) {
		if (artbook)
			*artbook = 1;
		return 4;
	}
	return 0;
}

int zlyme_bg_is_current(unsigned current, unsigned task_gen)
{
	return current != 0 && current == task_gen;
}
