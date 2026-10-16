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

	expect(zlyme_artbook_system(out, sizeof(out), "/usr/share/nextui/artbook", "GB") == 1, "gb");
	expect(strcmp(out, "/usr/share/nextui/artbook/system/(GB).png") == 0, out);
	expect(zlyme_artbook_tool(out, sizeof(out), "/art", "Music Player") == 1, "music");
	expect(strcmp(out, "/art/tools/Music Player.png") == 0, out);
	expect(zlyme_artbook_tool(out, sizeof(out), "/art", "USB File Transfer") == 1, "usb");
	expect(zlyme_artbook_system(out, sizeof(out), "/art", "GB/FC") == 0, "slash");
	expect(zlyme_artbook_system(out, sizeof(out), "/art", "../GB") == 0, "dotdot");
	expect(zlyme_artbook_tool(out, sizeof(out), "/art", "Files/Settings") == 0, "tool slash");
	expect(zlyme_artbook_system(out, sizeof(out), "/art", "") == 0, "empty");
	expect(zlyme_artbook_system(out, 8, "/art", "GB") == 0, "short");
	expect(strstr(out, "list") == NULL, "no list path");

	/* Request 1 is in flight. Navigation bumps the request to 2 and
	 * clears the picture. Request 1 then finishes and must not apply.
	 * Request 2 is the one that may.
	 */
	expect(zlyme_bg_is_current(1, 1) == 1, "in flight matches");
	expect(zlyme_bg_is_current(2, 1) == 0, "stale load after a clear");
	expect(zlyme_bg_is_current(2, 2) == 1, "latest load applies");
	expect(zlyme_bg_is_current(3, 2) == 0, "older queued load");
	expect(zlyme_bg_is_current(0, 0) == 0, "cleared generation");

	if (fails)
		return 1;
	printf("artbook path ok\n");
	return 0;
}
