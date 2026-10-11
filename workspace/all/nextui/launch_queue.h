#ifndef LAUNCH_QUEUE_H
#define LAUNCH_QUEUE_H

/*
 * /tmp/next is a text file, not a shell command.
 *
 *   ZLYME1\n
 *   1\n
 *   /absolute/pak/launch.sh\n
 *
 * or, when a ROM is present:
 *
 *   ZLYME1\n
 *   2\n
 *   /absolute/pak/launch.sh\n
 *   /path/to/rom\n
 *
 * The executable must be absolute and end in /launch.sh.
 * A newline, carriage return, or other ASCII control except tab is
 * rejected and nothing is written. The ROM is the next field, not
 * shell source. nextui-session reads the same framing.
 */

int launch_queue_write(const char *exe, const char *rom);
int launch_queue_write_file(const char *dest, const char *exe, const char *rom);

#endif
