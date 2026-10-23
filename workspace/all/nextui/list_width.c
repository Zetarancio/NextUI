#include "list_width.h"

int zlyme_list_row_width(int left, int clear_right, int gap,
	const int *edges, int nedges)
{
	int right = clear_right;
	int width;
	int i;

	if (gap < 0)
		gap = 0;
	if (edges && nedges > 0) {
		for (i = 0; i < nedges; i++) {
			int edge = edges[i] - gap;

			if (edge < right)
				right = edge;
		}
	}
	width = right - left;
	if (width < 1)
		return 1;
	return width;
}

void zlyme_pill_scroll(int available, int text_plus_pad, int inset,
	int *pill, int *scroll)
{
	int p;
	int s;

	if (available < 1)
		available = 1;
	if (text_plus_pad < 1)
		text_plus_pad = 1;
	p = text_plus_pad < available ? text_plus_pad : available;
	if (p < 1)
		p = 1;
	s = p - inset;
	if (s < 0)
		s = 0;
	if (s > p)
		s = p;
	if (pill)
		*pill = p;
	if (scroll)
		*scroll = s;
}

int zlyme_ranges_overlap(int a0, int a1, int b0, int b1)
{
	return a0 < b1 && b0 < a1;
}

void zlyme_rom_thumb_box(int screen_w, int screen_h, double art_frac,
	int img_w, int img_h, int margin3, int *x, int *y, int *w, int *h)
{
	int max_w;
	int max_h;
	int new_w;
	int new_h;
	int target_y;
	int top;

	if (art_frac < 0.0)
		art_frac = 0.0;
	if (art_frac > 1.0)
		art_frac = 1.0;
	if (screen_w < 0)
		screen_w = 0;
	if (screen_h < 0)
		screen_h = 0;
	max_w = (int)(screen_w * art_frac);
	max_h = (int)(screen_h * 0.6);
	new_w = max_w;
	new_h = max_h;
	if (img_w > 0 && img_h > 0 && max_w > 0) {
		double aspect = (double)img_h / (double)img_w;

		new_w = max_w;
		new_h = (int)(new_w * aspect);
		if (new_h > max_h && aspect > 0.0) {
			new_h = max_h;
			new_w = (int)(new_h / aspect);
		}
	}
	if (new_w < 0)
		new_w = 0;
	if (new_h < 0)
		new_h = 0;
	target_y = (int)(screen_h * 0.50);
	top = target_y - (new_h / 2);
	if (x)
		*x = screen_w - (new_w + margin3);
	if (y)
		*y = top;
	if (w)
		*w = new_w;
	if (h)
		*h = new_h;
}
