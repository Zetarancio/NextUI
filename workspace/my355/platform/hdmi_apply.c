#include "hdmi_mode.h"

#include <SDL2/SDL.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

int GetHDMI(void);

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define HDMI_MODE_MAX 64

static int letterbox;
static int view_x, view_y, view_w, view_h;
static int base_w, base_h;
static struct zlyme_hdmi_choice applied;

static int gate_on(void)
{
	const char *flag = getenv("ZLYME_HDMI_720P");

	return flag && flag[0] == '1' && flag[1] == '\0';
}

static int collect_drm(struct zlyme_hdmi_mode *out, int max, int *nlists,
		       struct zlyme_hdmi_mode **list_ptr, int *list_count)
{
	int card;
	int count = 0;

	*nlists = 0;
	for (card = 0; card < 2 && *nlists < 2; card++) {
		char path[64];
		int fd;
		drmModeRes *res;
		int i;
		int start = count;

		snprintf(path, sizeof(path), "/dev/dri/card%d", card);
		fd = open(path, O_RDWR | O_CLOEXEC);
		if (fd < 0)
			continue;
		res = drmModeGetResources(fd);
		if (!res) {
			close(fd);
			continue;
		}
		for (i = 0; i < res->count_connectors && count < max; i++) {
			drmModeConnector *conn;
			int m;

			conn = drmModeGetConnector(fd, res->connectors[i]);
			if (!conn)
				continue;
			if (conn->connection != DRM_MODE_CONNECTED ||
			    conn->connector_type != DRM_MODE_CONNECTOR_HDMIA ||
			    conn->count_modes <= 0) {
				drmModeFreeConnector(conn);
				continue;
			}
			start = count;
			for (m = 0; m < conn->count_modes && count < max; m++) {
				drmModeModeInfo *mode = &conn->modes[m];

				out[count].width = mode->hdisplay;
				out[count].height = mode->vdisplay;
				out[count].refresh_mhz = zlyme_hdmi_refresh_mhz(
					mode->clock, mode->htotal, mode->vtotal);
				if (out[count].refresh_mhz <= 0 && mode->vrefresh > 0)
					out[count].refresh_mhz = mode->vrefresh * 1000;
				out[count].preferred =
					(mode->type & DRM_MODE_TYPE_PREFERRED) ? 1 : 0;
				count++;
			}
			if (count > start && *nlists < 2) {
				list_ptr[*nlists] = &out[start];
				list_count[*nlists] = count - start;
				(*nlists)++;
			}
			drmModeFreeConnector(conn);
		}
		drmModeFreeResources(res);
		/* This open only reads modes. If it became master because
		 * nobody else held the device, release that before close.
		 * A card SDL already masters rejects the drop. */
		drmDropMaster(fd);
		close(fd);
	}
	return count;
}

static int sdl_mode_matches(const SDL_DisplayMode *mode,
			    const struct zlyme_hdmi_choice *want, int *delta)
{
	int refresh;

	if (mode->w != want->width || mode->h != want->height)
		return 0;
	refresh = mode->refresh_rate > 0 ? mode->refresh_rate * 1000 : 0;
	*delta = refresh - want->refresh_mhz;
	if (*delta < 0)
		*delta = -*delta;
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

void PLAT_hdmi_after_window(SDL_Window *window, SDL_Renderer *renderer)
{
	struct zlyme_hdmi_mode storage[HDMI_MODE_MAX];
	struct zlyme_hdmi_mode *lists[2];
	int counts[2];
	int nlists = 0;
	struct zlyme_hdmi_choice choice;
	struct zlyme_hdmi_rect box;
	SDL_DisplayMode mode;
	int display = -1;
	int index = -1;
	int best_delta = 0;
	int d, i, n;
	SDL_DisplayMode got;
	int orig_x = 0;
	int orig_y = 0;

	if (!gate_on() || !window || !renderer || !GetHDMI()) {
		letterbox = 0;
		return;
	}
	if (collect_drm(storage, HDMI_MODE_MAX, &nlists, lists, counts) <= 0 ||
	    zlyme_hdmi_select_list((const struct zlyme_hdmi_mode *const *)lists,
				   counts, nlists, &choice) < 0 ||
	    !choice.chosen) {
		letterbox = 0;
		return;
	}
	/* Same choice in this process: the CRTC request already happened. */
	if (zlyme_hdmi_same(&applied, &choice))
		return;
	letterbox = 0;
	for (d = 0; d < SDL_GetNumVideoDisplays(); d++) {
		n = SDL_GetNumDisplayModes(d);
		for (i = 0; i < n; i++) {
			int delta = 0;

			if (SDL_GetDisplayMode(d, i, &mode) != 0)
				continue;
			if (!sdl_mode_matches(&mode, &choice, &delta))
				continue;
			if (display < 0 || delta < best_delta) {
				display = d;
				index = i;
				best_delta = delta;
			}
		}
	}
	if (display < 0)
		return;
	/* KMSDRM creates one SDL display per connector and puts the
	 * window on display 0. SetWindowDisplayMode changes that
	 * display only, so the window has to move onto the connector
	 * that advertised the chosen mode. The driver hook ignores
	 * position; SDL still uses it to pick the display. */
	SDL_GetWindowPosition(window, &orig_x, &orig_y);
	SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED_DISPLAY(display),
			      SDL_WINDOWPOS_CENTERED_DISPLAY(display));
	if (SDL_GetWindowDisplayIndex(window) != display ||
	    SDL_GetDisplayMode(display, index, &mode) != 0) {
		SDL_SetWindowPosition(window, orig_x, orig_y);
		return;
	}
	if (SDL_SetWindowDisplayMode(window, &mode) != 0 ||
	    SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN) != 0 ||
	    SDL_GetWindowDisplayMode(window, &got) != 0 ||
	    got.w != choice.width || got.h != choice.height ||
	    SDL_GetWindowDisplayIndex(window) != display) {
		SDL_SetWindowFullscreen(window, 0);
		SDL_SetWindowPosition(window, orig_x, orig_y);
		return;
	}
	if (!zlyme_hdmi_commit(1, &choice, &applied)) {
		if (!zlyme_hdmi_same(&applied, &choice))
			return;
	}
	zlyme_hdmi_viewport(640, 480, got.w, got.h, &box);
	if (box.w == got.w && box.h == got.h) {
		letterbox = 0;
		return;
	}
	base_w = 640;
	base_h = 480;
	view_x = box.x;
	view_y = box.y;
	view_w = box.w;
	view_h = box.h;
	letterbox = 1;
	SDL_RenderSetLogicalSize(renderer, 640, 480);
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
	SDL_Log("ZLYME_HDMI_720P: %dx%d viewport %d,%d %dx%d",
		got.w, got.h, box.x, box.y, box.w, box.h);
}
