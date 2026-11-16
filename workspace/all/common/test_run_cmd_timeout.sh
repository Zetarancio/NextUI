#!/bin/sh
# Host regression for runCmdTimeout. Not a Flip run.
set -eu
here=$(CDPATH='' cd -- "$(dirname "$0")" && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

cat > "$work/platform.h" << 'EOF'
#ifndef PLATFORM_H_STUB
#define PLATFORM_H_STUB
#include <stdbool.h>
#define SDCARD_PATH "/storage"
#define PLATFORM "my355"
#define FIXED_WIDTH 640
#define FIXED_HEIGHT 480
#define FIXED_BPP 2
#define FIXED_SCALE 1
#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480
#define PAGE_COUNT 2
#define PAGE_SCALE 3
#endif
EOF

gcc -std=gnu99 -Wall -Wextra -pthread -ffunction-sections -fdata-sections \
	-I"$work" -I"$here" \
	-c -o "$work/utils.o" "$here/utils.c" 2>"$work/utils.warn"
if grep -E 'cmd_child|runCmdTimeout' "$work/utils.warn"; then
	echo "run-cmd: the helper did not compile cleanly" >&2
	exit 1
fi
gcc -std=gnu99 -Wall -Wextra -Werror -pthread -Wl,--gc-sections \
	-I"$here" \
	-o "$work/cmd" "$here/test_run_cmd_timeout.c" "$work/utils.o"
"$work/cmd"
