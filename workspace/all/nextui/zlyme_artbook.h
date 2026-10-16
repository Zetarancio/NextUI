#ifndef ZLYME_ARTBOOK_H
#define ZLYME_ARTBOOK_H

#include <stddef.h>

/* Build the firmware ArtBook path. list_mode selects the ROM-list image.
 * name is a system tag (GB) or Tools, Recently Played, or Collections.
 * Returns 1 when out holds a path. Does not check that the file exists.
 */
int zlyme_artbook_path(char *out, size_t out_len, const char *root, int list_mode, const char *name);

#endif
