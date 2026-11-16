#!/bin/sh
# Host threads for the Bluetooth flag primitives. Not a Flip run.
set -eu
here=$(CDPATH='' cd -- "$(dirname "$0")" && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

if ! gcc -std=c11 -Wall -Wextra -Werror -pthread -fsanitize=thread \
	-o "$work/race" "$here/test_bt_cache_race.c" -lSDL2; then
	echo "bt-cache-race: thread sanitizer did not link" >&2
	exit 1
fi
"$work/race"
