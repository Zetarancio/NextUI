#ifndef ZLYME_TEXTROW_H
#define ZLYME_TEXTROW_H

#include <stdio.h>
#include <string.h>

/* One overlay row. A row longer than dstsz is cut. Never writes past
 * dstsz - 1. The Settings reset dialog used to hand a path list to
 * GFX_blitMessage, and strcpy into char line[256] aborted.
 */
static inline size_t zlyme_copy_text_row(char *dst, size_t dstsz,
		const char *src, size_t len)
{
	if (!dst || dstsz == 0)
		return 0;
	if (!src)
		len = 0;
	if (len >= dstsz)
		len = dstsz - 1;
	if (len && src)
		memcpy(dst, src, len);
	dst[len] = '\0';
	return len;
}

/* Count-only confirmation. The cleanup script still prints the paths.
 * This dialog does not copy them in.
 */
static inline int zlyme_cleanup_ask(char *dst, size_t dstsz,
		const char *fmt, int count)
{
	int wrote;

	if (!dst || dstsz < 2 || !fmt || count < 1)
		return 0;
	wrote = snprintf(dst, dstsz, fmt, count);
	if (wrote < 0 || (size_t)wrote >= dstsz)
		return 0;
	return 1;
}

#endif
