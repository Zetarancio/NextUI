#include "layout.hpp"
#include <stdio.h>

/* my355: 640x480, FIXED_SCALE 2, PADDING 10, PILL_SIZE 30. */

int main(void)
{
	const int pad = 20;
	const int pill = 60;
	const ZlymeContentRect content = zlyme_settings_content(640, 480, pad, pill, pill, true, true);
	const int cap = zlyme_max_visible_rows(content.h, pill);
	const int with_devices = zlyme_visible_rows(3 + 8, content.h, pill);
	const int empty = zlyme_visible_rows(3, content.h, pill);
	const int last = zlyme_last_row_bottom(content, with_devices, pill);
	const int desc = zlyme_description_top(content, pill);
	const int hint = zlyme_hint_pill_top(480, pad, pill);

	if (cap < 4) {
		fprintf(stderr, "bt layout: only %d rows, the first device is hidden\n", cap);
		return 1;
	}
	if (empty != 3) {
		fprintf(stderr, "bt layout: empty scan is not the three fixed rows\n");
		return 1;
	}
	if (with_devices != cap) {
		fprintf(stderr, "bt layout: extra devices do not scroll\n");
		return 1;
	}
	if (last > desc || zlyme_description_bottom(content) > hint) {
		fprintf(stderr, "bt layout: row %d desc %d hint %d overlap\n", last, desc, hint);
		return 1;
	}
	printf("bt layout ok rows=%d\n", cap);
	return 0;
}
