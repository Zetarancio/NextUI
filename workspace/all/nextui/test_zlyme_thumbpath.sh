#!/bin/sh
set -eu
cd "$(dirname "$0")"
gcc -std=c11 -Wall -Wextra -Werror -o /tmp/zlyme-thumbpath-test \
	zlyme_thumbpath.c test_zlyme_thumbpath.c
/tmp/zlyme-thumbpath-test
rm -f /tmp/zlyme-thumbpath-test
# The cartridge preview stays behind .media, and only for a finished .p8.png.
awk '
	/zlyme_game_art_path\(thumbpath/ { art = 1 }
	art && /suffixMatch\("\.p8\.png"/ { found = 1; exit }
	END { exit found ? 0 : 1 }
' nextui.c || {
	echo "pico-8 preview is no longer behind the media path" >&2
	exit 1
}
