#include "hdmi_mode.h"

#include <stdlib.h>
#include <string.h>

static int usable(const struct zlyme_hdmi_mode *mode)
{
	return mode->width > 0 && mode->height > 0 && mode->refresh_mhz > 0;
}

static int near_60(int refresh_mhz)
{
	int delta = refresh_mhz - 60000;

	if (delta < 0)
		delta = -delta;
	return delta <= 500;
}

static int is_720p60(const struct zlyme_hdmi_mode *mode)
{
	return usable(mode) && mode->width == 1280 && mode->height == 720 &&
	       near_60(mode->refresh_mhz);
}

int zlyme_hdmi_refresh_mhz(int clock_khz, int htotal, int vtotal)
{
	long long denom;

	if (clock_khz <= 0 || htotal <= 0 || vtotal <= 0)
		return 0;
	denom = (long long)htotal * (long long)vtotal;
	if (denom <= 0)
		return 0;
	return (int)(((long long)clock_khz * 1000000LL) / denom);
}

void zlyme_hdmi_choose(const struct zlyme_hdmi_mode *modes, int count,
		       struct zlyme_hdmi_choice *out)
{
	int i;
	int best = -1;
	int best_delta = 0;

	out->width = 0;
	out->height = 0;
	out->refresh_mhz = 0;
	out->chosen = 0;
	if (!modes || count <= 0)
		return;

	for (i = 0; i < count; i++) {
		int delta;

		if (!is_720p60(&modes[i]))
			continue;
		delta = modes[i].refresh_mhz - 60000;
		if (delta < 0)
			delta = -delta;
		if (best < 0 || delta < best_delta ||
		    (delta == best_delta && modes[i].preferred &&
		     !modes[best].preferred)) {
			best = i;
			best_delta = delta;
		}
	}
	if (best < 0) {
		for (i = 0; i < count; i++) {
			if (usable(&modes[i]) && modes[i].preferred) {
				best = i;
				break;
			}
		}
	}
	if (best < 0) {
		for (i = 0; i < count; i++) {
			if (usable(&modes[i])) {
				best = i;
				break;
			}
		}
	}
	if (best < 0)
		return;
	out->width = modes[best].width;
	out->height = modes[best].height;
	out->refresh_mhz = modes[best].refresh_mhz;
	out->chosen = 1;
}

int zlyme_hdmi_select_list(const struct zlyme_hdmi_mode *const *lists,
			   const int *counts, int nlists,
			   struct zlyme_hdmi_choice *out)
{
	int i;
	int fallback = -1;
	struct zlyme_hdmi_choice cur;

	out->width = 0;
	out->height = 0;
	out->refresh_mhz = 0;
	out->chosen = 0;
	if (!lists || !counts || nlists <= 0)
		return -1;
	for (i = 0; i < nlists; i++) {
		zlyme_hdmi_choose(lists[i], counts[i], &cur);
		if (!cur.chosen)
			continue;
		if (is_720p60(&(struct zlyme_hdmi_mode){
			    .width = cur.width,
			    .height = cur.height,
			    .refresh_mhz = cur.refresh_mhz,
			    .preferred = 0})) {
			*out = cur;
			return i;
		}
		if (fallback < 0)
			fallback = i;
	}
	if (fallback < 0)
		return -1;
	zlyme_hdmi_choose(lists[fallback], counts[fallback], out);
	return fallback;
}

int zlyme_hdmi_same(const struct zlyme_hdmi_choice *a,
		    const struct zlyme_hdmi_choice *b)
{
	if (!a || !b)
		return 0;
	return a->chosen && b->chosen && a->width == b->width &&
	       a->height == b->height && a->refresh_mhz == b->refresh_mhz;
}

int zlyme_hdmi_commit(int modeset_ok, const struct zlyme_hdmi_choice *next,
		      struct zlyme_hdmi_choice *active)
{
	if (!modeset_ok || !next || !active || !next->chosen)
		return 0;
	if (zlyme_hdmi_same(active, next))
		return 0;
	*active = *next;
	return 1;
}

void zlyme_hdmi_viewport(int logical_w, int logical_h, int out_w, int out_h,
			 struct zlyme_hdmi_rect *out)
{
	int w;
	int h;

	out->x = 0;
	out->y = 0;
	out->w = 0;
	out->h = 0;
	if (logical_w <= 0 || logical_h <= 0 || out_w <= 0 || out_h <= 0)
		return;
	w = logical_w * out_h / logical_h;
	if (w <= out_w) {
		h = out_h;
	} else {
		w = out_w;
		h = logical_h * out_w / logical_w;
	}
	if (w < 1)
		w = 1;
	if (h < 1)
		h = 1;
	out->w = w;
	out->h = h;
	out->x = (out_w - w) / 2;
	out->y = (out_h - h) / 2;
}

int zlyme_panel_refresh_hz(const char *saved)
{
	if (saved && (strcmp(saved, "40") == 0 || strcmp(saved, "50") == 0 ||
		      strcmp(saved, "60") == 0))
		return atoi(saved);
	return 60;
}
