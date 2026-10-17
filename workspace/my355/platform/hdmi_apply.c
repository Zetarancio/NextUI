#include "hdmi_mode.h"

#include <SDL2/SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int letterbox;
static int view_x, view_y, view_w, view_h;
static int base_w, base_h;
static int scanout_done;
static char scan_card[8];
static char scan_name[32];
static int scan_w, scan_h;

static int gate_on(void)
{
	const char *flag = getenv("ZLYME_HDMI_720P");

	return flag && flag[0] == '1' && flag[1] == '\0';
}

static int env_int(const char *name)
{
	const char *value = getenv(name);
	char *end = NULL;
	long parsed;

	if (!value || !value[0])
		return -1;
	parsed = strtol(value, &end, 10);
	if (!end || *end != '\0' || parsed < 0 || parsed > 16384)
		return -1;
	return (int)parsed;
}

static int name_ok(const char *name)
{
	int i;

	if (!name || !name[0] || strlen(name) > 31)
		return 0;
	for (i = 0; name[i]; i++) {
		char c = name[i];

		if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
		      (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.'))
			return 0;
	}
	return 1;
}

void PLAT_hdmi_map_output(SDL_Rect *rect)
{
	if (!letterbox || !rect || base_w <= 0 || base_h <= 0)
		return;
	rect->x = view_x + rect->x * view_w / base_w;
	rect->y = view_y + rect->y * view_h / base_h;
	rect->w = rect->w * view_w / base_w;
	rect->h = rect->h * view_h / base_h;
}

int PLAT_hdmi_bars(void)
{
	return letterbox;
}

/* The CRTC ioctl runs on the first swap, after this function returns.
 * SDL_GetCurrentDisplayMode is only the mode SDL stored. The sysfs
 * mode file is the connector's current WxH. It does not carry the
 * pixel clock, so it cannot separate 59.94 Hz from 60 Hz. */
void PLAT_hdmi_note_present(void)
{
	char path[96];
	char got[64];
	FILE *fp;
	int n;

	if (scanout_done || !letterbox)
		return;
	scanout_done = 1;
	n = snprintf(path, sizeof(path), "/sys/class/drm/card%s-%s/mode",
		     scan_card, scan_name);
	if (n < 0 || n >= (int)sizeof(path)) {
		SDL_Log("ZLYME_HDMI_SCANOUT unread");
		return;
	}
	fp = fopen(path, "r");
	if (!fp) {
		SDL_Log("ZLYME_HDMI_SCANOUT unread %s", path);
		return;
	}
	if (!fgets(got, sizeof(got), fp)) {
		fclose(fp);
		SDL_Log("ZLYME_HDMI_SCANOUT unread %s", path);
		return;
	}
	fclose(fp);
	got[strcspn(got, "\r\n")] = '\0';
	if (atoi(got) == scan_w) {
		char *x = strchr(got, 'x');

		if (x && atoi(x + 1) == scan_h) {
			SDL_Log("ZLYME_HDMI_SCANOUT %s %s", path, got);
			return;
		}
	}
	SDL_Log("ZLYME_HDMI_SCANOUT mismatch %s %s", path, got);
}

void PLAT_hdmi_after_window(SDL_Window *window, SDL_Renderer *renderer)
{
	struct zlyme_hdmi_rect box;
	SDL_DisplayMode got;
	const char *name;
	int width, height, card, index;

	letterbox = 0;
	scanout_done = 0;
	if (!gate_on() || !window || !renderer)
		return;
	width = env_int("ZLYME_HDMI_WIDTH");
	height = env_int("ZLYME_HDMI_HEIGHT");
	card = env_int("ZLYME_HDMI_CARD");
	name = getenv("ZLYME_HDMI_CONNECTOR");
	if (width <= 0 || height <= 0 || card < 0 || card > 9 || !name_ok(name))
		return;
	index = SDL_GetWindowDisplayIndex(window);
	if (index < 0 || SDL_GetCurrentDisplayMode(index, &got) != 0)
		return;
	/* The SDL patch selected this size on the connector id it was
	 * given. A size match on a different connector is not success:
	 * the patch refuses to program a non-HDMI-A connector. */
	if (got.w != width || got.h != height) {
		SDL_Log("ZLYME_HDMI_REQUEST mismatch sdl %dx%d policy %dx%d",
			got.w, got.h, width, height);
		return;
	}
	zlyme_hdmi_viewport(640, 480, got.w, got.h, &box);
	snprintf(scan_card, sizeof(scan_card), "%d", card);
	snprintf(scan_name, sizeof(scan_name), "%s", name);
	scan_w = got.w;
	scan_h = got.h;
	SDL_Log("ZLYME_HDMI_REQUEST %dx%d viewport %d,%d %dx%d",
		got.w, got.h, box.x, box.y, box.w, box.h);
	if (box.w == got.w && box.h == got.h)
		return;
	base_w = 640;
	base_h = 480;
	view_x = box.x;
	view_y = box.y;
	view_w = box.w;
	view_h = box.h;
	letterbox = 1;
	SDL_RenderSetLogicalSize(renderer, 640, 480);
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
}