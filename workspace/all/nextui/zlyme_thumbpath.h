#ifndef ZLYME_THUMBPATH_H
#define ZLYME_THUMBPATH_H

#include <stddef.h>

/* Game-list art for one ROM stem.
 * An existing .png wins. Otherwise .jpg, then .jpeg.
 * When none of those files exist, the .png path is still written so the
 * caller can apply its own fallback. Returns 0 when the path does not fit. */
int zlyme_game_art_path(char *out, size_t out_len, const char *romdir, const char *name);

#endif
