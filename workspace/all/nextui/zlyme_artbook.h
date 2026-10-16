#ifndef ZLYME_ARTBOOK_H
#define ZLYME_ARTBOOK_H

#include <stddef.h>

/* Firmware ArtBook paths. tag is a system tag such as GB.
 * pak is a Tools pak basename without ".pak", and may contain spaces.
 * Returns 1 when out holds a path. Does not check that the file exists.
 */
int zlyme_artbook_system(char *out, size_t out_len, const char *root, const char *tag);
int zlyme_artbook_tool(char *out, size_t out_len, const char *root, const char *pak);

/* 1 when task_gen is the newest background request. 0 is never current,
 * so a cleared request cannot be restored by an older load.
 */
int zlyme_bg_is_current(unsigned current, unsigned task_gen);

#endif
