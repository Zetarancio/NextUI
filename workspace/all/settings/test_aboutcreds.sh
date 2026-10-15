#!/bin/sh
# Host checks for Settings → About credential labels.
# shellcheck disable=SC1007
set -eu
here=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
settings=$here/settings.cpp
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

fail() {
	printf 'about: %s\n' "$*" >&2
	exit 1
}

g++ -std=c++17 -Wall -Wextra -Werror -I"$here" \
	-o "$work/aboutcreds" "$here/aboutcreds.cpp" "$here/test_aboutcreds.cpp"
"$work/aboutcreds"

# The seven rows stay in this order. Other About rows may sit around them.
awk '
	/"SSH Account"/ { rows[++n] = "SSH Account" }
	/"SSH Password"/ { rows[++n] = "SSH Password" }
	/"Samba Account"/ { rows[++n] = "Samba Account" }
	/"Samba Password"/ { rows[++n] = "Samba Password" }
	/"Syncthing Account"/ { rows[++n] = "Syncthing Account" }
	/"Syncthing Password"/ { rows[++n] = "Syncthing Password" }
	/"IP"/ { rows[++n] = "IP" }
	END {
		want[1] = "SSH Account"
		want[2] = "SSH Password"
		want[3] = "Samba Account"
		want[4] = "Samba Password"
		want[5] = "Syncthing Account"
		want[6] = "Syncthing Password"
		want[7] = "IP"
		if (n != 7) exit 1
		for (i = 1; i <= 7; i++) if (rows[i] != want[i]) exit 1
	}
' "$settings" || fail "About row order"

# Opening About refreshes. Drawing a row returns the saved text.
grep -q 'void onShow() override { about_refresh(\*snap); }' "$settings" \
	|| fail "About does not refresh on entry"
if awk '
	/new StaticMenuItem\{ListItemType::Generic, "SSH Account"/ { cred = 1 }
	cred && /new StaticMenuItem\{ListItemType::Generic, "BusyBox"/ { cred = 0 }
	cred && /Zlyme_sshPassword|zlyme_about_|wlan_ip|read_about_file/ { bad = 1 }
	END { exit bad ? 0 : 1 }
' "$settings"; then
	fail "About rows read credentials while drawing"
fi
grep -q 'snap.ssh_password = Zlyme_sshPasswordDisplay();' "$settings" \
	|| fail "About does not read the SSH password on entry"

echo "about creds ok"
