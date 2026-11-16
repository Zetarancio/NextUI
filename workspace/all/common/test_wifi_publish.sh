#!/bin/sh
# Host category and publication check. Not a Flip run.
set -eu
here=$(CDPATH='' cd -- "$(dirname "$0")" && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

if ! gcc -std=c11 -Wall -Wextra -Werror -pthread -fsanitize=thread \
	-o "$work/wifi" "$here/test_wifi_publish.c" -lSDL2; then
	echo "wifi-publish: thread sanitizer did not link" >&2
	exit 1
fi
i=0
while [ "$i" -lt 20 ]; do
	"$work/wifi"
	i=$((i + 1))
done
