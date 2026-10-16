#include "zlyme_artbook.h"

#include <stdio.h>
#include <string.h>

static int fails;

static void expect(int cond, const char *msg)
{
	if (!cond) {
		fprintf(stderr, "artbook: %s\n", msg);
		fails++;
	}
}

int main(void)
{
	char out[256];

	expect(zlyme_artbook_path(out, sizeof(out), "/usr/share/nextui/artbook", 0, "GB") == 1, "gb system");
	expect(strcmp(out, "/usr/share/nextui/artbook/system/(GB).png") == 0, out);
	expect(zlyme_artbook_path(out, sizeof(out), "/usr/share/nextui/artbook", 1, "GBA") == 1, "gba list");
	expect(strcmp(out, "/usr/share/nextui/artbook/list/(GBA)-list.png") == 0, out);
	expect(zlyme_artbook_path(out, sizeof(out), "/art", 0, "Tools") == 1, "tools");
	expect(strcmp(out, "/art/system/Tools.png") == 0, out);
	expect(zlyme_artbook_path(out, sizeof(out), "/art", 1, "Recently Played") == 1, "recents list");
	expect(strcmp(out, "/art/list/Recently Played-list.png") == 0, out);
	expect(zlyme_artbook_path(out, sizeof(out), "/art", 0, "Collections") == 1, "collections");
	expect(strcmp(out, "/art/system/Collections.png") == 0, out);
	expect(zlyme_artbook_path(out, sizeof(out), "/art", 0, "../GB") == 0, "dotdot");
	expect(zlyme_artbook_path(out, sizeof(out), "/art", 0, "GB/FC") == 0, "slash");
	expect(zlyme_artbook_path(out, sizeof(out), "/art", 0, "") == 0, "empty");
	expect(zlyme_artbook_path(out, 8, "/art", 0, "GB") == 0, "short buffer");

	if (fails)
		return 1;
	printf("artbook path ok\n");
	return 0;
}
