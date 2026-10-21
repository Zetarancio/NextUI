#ifndef ZLYME_ARTBOOK_H
#define ZLYME_ARTBOOK_H

#include <stddef.h>

/* Firmware ArtBook paths. tag is a system tag such as GB.
 * pak is a Tools pak basename without ".pak", and may contain spaces.
 * Returns 1 when out holds a path. Does not check that the file exists.
 */
int zlyme_artbook_system(char *out, size_t out_len, const char *root, const char *tag);
int zlyme_artbook_tool(char *out, size_t out_len, const char *root, const char *pak);
/* The Tools root category. Individual Tool paks use zlyme_artbook_tool. */
int zlyme_artbook_tools_category(char *out, size_t out_len, const char *root);
/* Firmware Recently Played picture. The folder on the card can be absent. */
int zlyme_artbook_recent(char *out, size_t out_len, const char *root);
/* Optional picture under the NextUI shared config directory. */
int zlyme_recent_user_path(char *out, size_t out_len, const char *shared);
/* 1 user override, 2 card background, 3 firmware.
 * *artbook is 1 only for the firmware file. 0 when none of them exist.
 */
int zlyme_recent_bg_choice(int user, int global, int firmware, int *artbook);

/* 1 when task_gen is the newest background request. 0 is never current,
 * so a cleared request cannot be restored by an older load.
 */
int zlyme_bg_is_current(unsigned current, unsigned task_gen);

#endif
