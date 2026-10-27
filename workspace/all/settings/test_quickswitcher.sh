#!/bin/sh
# my355 hides the Quickswitcher switch and still draws the controls.
# shellcheck disable=SC1007
set -eu
here=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
root=$(CDPATH= cd -- "$here/../../.." && pwd)
fail() { printf 'quickswitcher: %s\n' "$*" >&2; exit 1; }

settings=$root/workspace/all/settings/settings.cpp
config=$root/workspace/all/common/config.c
menu=$root/workspace/all/nextui/nextui.c
zlyme=$here/zlymemenu.cpp

awk '
	/Show Quickswitcher UI/ { found=1; if (guard != 1) bad=1 }
	/deviceInfo.getPlatform\(\) != DeviceInfo::my355/ { guard=1 }
	END { if (!found || bad) exit 1 }
' "$settings" || fail "the setting is not limited to other platforms"

awk '
	/CFG_getShowQuickswitcherUI/ { fn=1 }
	fn && /strcmp\(PLATFORM, "my355"\) == 0/ { mine=1 }
	fn && mine && /return true;/ { ok=1 }
	fn && /return settings.showQuickSwitcherUi;/ { if (!ok) early=1; fn=0 }
	END { if (!ok || early) exit 1 }
' "$config" || fail "a saved 0 can still hide the my355 controls"

grep -q 'CFG_getShowQuickswitcherUI()' "$menu" || fail "the menu no longer asks the getter"
grep -q 'cleanup_button("orphan-prefs"' "$zlyme" || fail "orphan cleanup changed"
grep -q 'Remove settings for games that no longer exist.' "$zlyme" || fail "orphan description"
if grep -q 'Folder and system choices stay' "$zlyme"; then
	fail "the long orphan description is still in the menu"
fi

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cat > "$work/q.c" << 'EOF'
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
struct { bool showQuickSwitcherUi; } settings;
static bool shown(const char *platform)
{
	if (strcmp(platform, "my355") == 0)
		return true;
	return settings.showQuickSwitcherUi;
}
int main(void)
{
	settings.showQuickSwitcherUi = false;
	if (!shown("my355"))
		return 1;
	if (shown("tg5040"))
		return 2;
	settings.showQuickSwitcherUi = true;
	if (!shown("tg5040"))
		return 3;
	if (!shown("my355"))
		return 4;
	return 0;
}
EOF
cc -Wall -Wextra -Werror -o "$work/q" "$work/q.c"
"$work/q"
echo "quickswitcher ok"
