#ifndef NEXTUI_HDMI_MODE_H
#define NEXTUI_HDMI_MODE_H

/* refresh_mhz is millihertz. 60 Hz is 60000. 59.94 Hz is 59940. */
struct zlyme_hdmi_mode {
	int width;
	int height;
	int refresh_mhz;
	int preferred;
};

struct zlyme_hdmi_choice {
	int width;
	int height;
	int refresh_mhz;
	int chosen;
};

struct zlyme_hdmi_rect {
	int x;
	int y;
	int w;
	int h;
};

/* clock_khz is the DRM mode clock. 0 when the timing is incomplete. */
int zlyme_hdmi_refresh_mhz(int clock_khz, int htotal, int vtotal);

void zlyme_hdmi_choose(const struct zlyme_hdmi_mode *modes, int count,
		       struct zlyme_hdmi_choice *out);

/* Separate connector lists. The list that can show 720p near 60 Hz wins.
 * Lists are not merged. */
int zlyme_hdmi_select_list(const struct zlyme_hdmi_mode *const *lists,
			   const int *counts, int nlists,
			   struct zlyme_hdmi_choice *out);

int zlyme_hdmi_same(const struct zlyme_hdmi_choice *a,
		    const struct zlyme_hdmi_choice *b);

/* modeset_ok == 0 leaves active unchanged and returns 0. */
int zlyme_hdmi_commit(int modeset_ok, const struct zlyme_hdmi_choice *next,
		      struct zlyme_hdmi_choice *active);

/* Center logical_w x logical_h inside the output, preserving aspect. */
void zlyme_hdmi_viewport(int logical_w, int logical_h, int out_w, int out_h,
			 struct zlyme_hdmi_rect *out);

int zlyme_panel_refresh_hz(const char *saved);

#endif
