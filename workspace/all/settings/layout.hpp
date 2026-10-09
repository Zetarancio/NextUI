#pragma once

// Settings content rectangle. The hint pills are drawn on the full
// screen, with their top at screen_h - padding - pill. The initial
// content box already excludes padding, so the hint reservation is
// the pill height only.

struct ZlymeContentRect {
	int x;
	int y;
	int w;
	int h;
};

inline ZlymeContentRect zlyme_settings_content(
	int screen_w, int screen_h,
	int padding_px, int title_px, int footer_px,
	bool reserve_title, bool reserve_hints)
{
	ZlymeContentRect r{
		padding_px,
		padding_px,
		screen_w - padding_px * 2,
		screen_h - padding_px * 2};
	if (reserve_title) {
		r.y += title_px;
		r.h -= title_px;
	}
	if (reserve_hints)
		r.h -= footer_px;
	return r;
}

inline int zlyme_hint_pill_top(int screen_h, int padding_px, int pill_px)
{
	return screen_h - padding_px - pill_px;
}

// MenuList keeps one row of the content rect for the description.
inline int zlyme_max_visible_rows(int content_h, int row_px)
{
	int n = 1;
	if (row_px > 0)
		n = (content_h - row_px) / row_px;
	if (n < 1)
		n = 1;
	return n;
}

inline int zlyme_visible_rows(int item_count, int content_h, int row_px)
{
	int cap = zlyme_max_visible_rows(content_h, row_px);
	if (item_count < cap)
		return item_count > 0 ? item_count : 0;
	return cap;
}

inline int zlyme_last_row_bottom(const ZlymeContentRect &content, int rows, int row_px)
{
	return content.y + rows * row_px;
}

inline int zlyme_description_top(const ZlymeContentRect &content, int row_px)
{
	return content.y + content.h - row_px;
}

inline int zlyme_description_bottom(const ZlymeContentRect &content)
{
	return content.y + content.h;
}
