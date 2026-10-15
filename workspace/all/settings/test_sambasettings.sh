#!/bin/sh
# Host checks for the Samba password row and a visible start failure.
# shellcheck disable=SC1007
set -eu
here=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
menu=$here/zlymemenu.cpp
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

fail() {
	printf 'samba settings: %s\n' "$*" >&2
	exit 1
}

g++ -std=c++17 -Wall -Wextra -Werror -I"$here" \
	-o "$work/sambasettings" \
	"$here/sambasettings.cpp" "$here/aboutcreds.cpp" "$here/test_sambasettings.cpp"
"$work/sambasettings"

awk '
	/Zlyme_appendNetworkItems/ { in_fn=1 }
	/Zlyme_appendStatusLed/ { in_fn=0 }
	in_fn && /TextInputMenuItem\{"SSH Password"/ { rows[++n] = "SSH Password" }
	in_fn && /MenuItem\{ListItemType::Button, "Clear SSH password"/ { rows[++n] = "Clear SSH password" }
	in_fn && /MenuItem\{ListItemType::Generic, "SSH"/ { rows[++n] = "SSH" }
	in_fn && /MenuItem\{ListItemType::Generic, "Samba"/ { rows[++n] = "Samba" }
	in_fn && /TextInputMenuItem\{"Samba Password"/ { rows[++n] = "Samba Password" }
	in_fn && /MenuItem\{ListItemType::Generic, "Syncthing"/ { rows[++n] = "Syncthing" }
	END {
		want[1] = "SSH"
		want[2] = "SSH Password"
		want[3] = "Clear SSH password"
		want[4] = "Samba"
		want[5] = "Samba Password"
		want[6] = "Syncthing"
		if (n != 6) exit 1
		for (i = 1; i <= 6; i++) if (rows[i] != want[i]) exit 1
	}
' "$menu" || fail "network row order"

grep -q 'samba_prompt->setInitialText("")' "$menu" || fail "Samba password is prefilled"
grep -q 'zlyme_tool_exec("zlyme-samba-auth"' "$menu" || fail "Samba password does not use the auth tool"
if grep -n 'system(.*samba' "$menu"; then
	fail "Samba password is built into a shell command"
fi

echo "samba settings order ok"
