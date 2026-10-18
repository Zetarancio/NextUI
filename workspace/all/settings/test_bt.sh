#!/bin/sh
# Host checks for Bluetooth discovery policy and the 640x480 settings box.
set -eu
here=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
fail() { echo "bt: $*" >&2; exit 1; }

cc -std=c11 -Wall -Wextra -Werror -o "$work/policy" "$here/test_bt_policy.c"
"$work/policy"
c++ -std=c++17 -Wall -Wextra -Werror -I"$here" -o "$work/layout" "$here/test_bt_layout.cpp"
"$work/layout"

bt=$here/../common/generic_bt.c
agent=$here/btagent.cpp
menu=$here/btmenu.cpp
grep -q 'dbus-send' "$bt" && fail "discovery still uses dbus-send"
grep -q 'killall' "$bt" && fail "discovery still kills bluetoothctl"
grep -q 'bt_disc_set' "$bt" || fail "no persistent discovery owner"
grep -q 'bt_mac_ok' "$bt" || fail "mac is not validated"
grep -q 'GetManagedObjects' "$agent" || fail "scan still depends on bluetoothctl devices"
grep -q 'StartDiscovery' "$agent" || fail "agent does not start discovery"
grep -q 'StopDiscovery' "$agent" || fail "agent does not stop discovery"
grep -q 'BT_DBUS_MS' "$agent" || fail "dbus calls can block forever"
grep -q 'scan_same' "$menu" || fail "menu ignores a renamed device"
grep -q 'ASSET_BLUETOOTH' "$menu" || fail "unknown devices have no icon"
echo "bt host ok"
