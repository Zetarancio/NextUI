#include "list_width.h"
#include "config.h"

#include <stdio.h>

static int fails;

static void expect(int cond, const char *msg)
{
	if (!cond) {
		fprintf(stderr, "list-width: %s\n", msg);
		fails++;
	}
}

/* Miyoo Flip pixels at FIXED_SCALE 2. */
enum {
	SW = 640,
	SH = 480,
	LEFT = 10,
	CLEAR = 600,
	GAP = 10,
	HINT = 372,
	ART = 320,
	ROW0_TOP = 20,
	ROW0_BOT = 80,
	ROW1_TOP = 80,
	ROW1_BOT = 140,
	HINT_TOP = 20,
	HINT_BOT = 80
};

static int row_width(int hint_on, int hint, int thumb_on, int thumb, int art_on, int art)
{
	int edges[3];
	int n = 0;

	if (hint_on)
		edges[n++] = hint;
	if (thumb_on)
		edges[n++] = thumb;
	if (art_on)
		edges[n++] = art;
	return zlyme_list_row_width(LEFT, CLEAR, GAP, edges, n);
}

int main(void)
{
	int pill = 0;
	int scroll = 0;
	int x = 0, y = 0, w = 0, h = 0;
	int first;
	int later;
	int thumb_left;
	int thumb_top;
	int thumb_bot;

	/* First row, no thumbnail: stop at the hardware cluster. */
	first = row_width(1, HINT, 0, 0, 0, 0);
	later = row_width(0, 0, 0, 0, 0, 0);
	expect(first >= 120, "first row collapsed");
	expect(later > first, "later row is not wider than the first");
	expect(first == HINT - GAP - LEFT, "first row misses the cluster");
	expect(later == CLEAR - LEFT, "later row is not the clear width");

	/* ROM thumbnail. The first row does not meet a centered picture. */
	zlyme_rom_thumb_box(SW, SH, 0.45, 0, 0, 30, &x, &y, &w, &h);
	thumb_left = x;
	thumb_top = y;
	thumb_bot = y + h;
	expect(thumb_left == 322, "default art column");
	expect(!zlyme_ranges_overlap(ROW0_TOP, ROW0_BOT, thumb_top, thumb_bot),
		"first row meets the thumbnail");
	expect(zlyme_ranges_overlap(ROW1_TOP, ROW1_BOT, thumb_top, thumb_bot),
		"later row misses the thumbnail");
	expect(row_width(1, HINT, 0, 0, 0, 0) == first, "first row with a thumbnail left the cluster");
	later = row_width(0, 0, 1, thumb_left, 0, 0);
	expect(later == thumb_left - GAP - LEFT, "later row misses the thumbnail edge");
	expect(later > 200, "thumbnail row is not usable");
	expect(later < thumb_left - LEFT, "thumbnail row paints the picture");

	/* Art Book, no ROM thumbnail. Both rows stop at the safe edge.
	 * The cluster sits inside the picture, so it is not subtracted again. */
	first = row_width(1, HINT, 0, 0, 1, ART);
	later = row_width(0, 0, 0, 0, 1, ART);
	expect(first == later, "Art Book rows disagree");
	expect(first == ART - GAP - LEFT, "Art Book row misses the safe edge");
	expect(first > HINT - GAP - LEFT - 200, "Art Book row subtracted the cluster");

	/* User background is not an Art Book reservation. */
	first = row_width(1, HINT, 0, 0, 0, 0);
	later = row_width(0, 0, 0, 0, 0, 0);
	expect(first == HINT - GAP - LEFT, "user background hid the first row");
	expect(later == CLEAR - LEFT, "user background reserved Art Book space");

	/* Short text hugs the text. Long text fills the row and the scroll
	 * stays inside the pill. */
	zlyme_pill_scroll(first, 80, 48, &pill, &scroll);
	expect(pill == 80, "short text grew to the row");
	expect(scroll == 32, "short scroll left the text");
	expect(scroll <= pill, "short scroll wider than the pill");
	zlyme_pill_scroll(first, 4000, 48, &pill, &scroll);
	expect(pill == first, "long text left the row");
	expect(scroll == first - 48, "long scroll is not the pill interior");
	expect(scroll > 0 && scroll < pill, "long scroll is outside the pill");
	zlyme_pill_scroll(0, 0, 48, &pill, &scroll);
	expect(pill == 1, "empty pill");
	expect(scroll == 0, "empty scroll");

	/* Game art width extremes. */
	zlyme_rom_thumb_box(SW, SH, 0.0, 0, 0, 30, &x, &y, &w, &h);
	expect(w == 0, "minimum art width still reserves a picture");
	expect(row_width(0, 0, 1, x, 0, 0) > 500, "minimum art width collapses the row");
	zlyme_rom_thumb_box(SW, SH, 1.0, 0, 0, 30, &x, &y, &w, &h);
	expect(w == SW, "maximum art width is not the screen");
	expect(row_width(0, 0, 1, x, 0, 0) == 1, "maximum art width went negative");

	/* A tall picture still uses the same box the draw uses. */
	zlyme_rom_thumb_box(SW, SH, 0.45, 100, 400, 30, &x, &y, &w, &h);
	expect(h == (int)(SH * 0.6), "tall picture ignored the height cap");
	expect(w > 0 && x == SW - (w + 30), "tall picture left edge drifted");

	expect(zlyme_game_art_percent(0.45) == 45, "0.45 displays as 0");
	expect(zlyme_game_art_percent(0.0) == 0, "zero art percent");
	expect(zlyme_game_art_percent(1.0) == 100, "full art percent");
	expect(zlyme_game_art_percent(-1.0) == 0, "negative art percent");
	expect(zlyme_game_art_percent(1.5) == 100, "over-range art percent");

	if (fails) {
		fprintf(stderr, "list-width: %d failed\n", fails);
		return 1;
	}
	return 0;
}
