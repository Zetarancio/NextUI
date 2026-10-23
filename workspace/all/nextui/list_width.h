#ifndef ZLYME_LIST_WIDTH_H
#define ZLYME_LIST_WIDTH_H

/* Shared list-row geometry. Pixels are the caller's, already scaled.
 * edges are the left x of each obstacle this row actually meets.
 * An empty list leaves the clear right edge. The width is at least 1
 * and does not cross an edge to get there. */

int zlyme_list_row_width(int left, int clear_right, int gap,
	const int *edges, int nedges);

/* pill is the selected surface width. scroll is the texture width
 * inside that surface. Both are >= 0. scroll is never wider than pill.
 * A pill that would be zero becomes 1 so SDL is not given an empty
 * surface. A scroll that would be negative becomes 0 and the caller
 * skips the texture. */
void zlyme_pill_scroll(int available, int text_plus_pad, int inset,
	int *pill, int *scroll);

int zlyme_ranges_overlap(int a0, int a1, int b0, int b1);

/* ROM thumbnail box. art_frac is the Game art width setting, 0..1.
 * img_w or img_h of 0 uses the full configured column. The box matches
 * the on-screen draw. */
void zlyme_rom_thumb_box(int screen_w, int screen_h, double art_frac,
	int img_w, int img_h, int margin3, int *x, int *y, int *w, int *h);

#endif
