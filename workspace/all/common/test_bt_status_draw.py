#!/usr/bin/env python3
"""Source check for the status-pill Bluetooth rune.

This does not draw the pill and does not open sysfs.
"""
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
API = ROOT / "all" / "common" / "api.c"
API_H = ROOT / "all" / "common" / "api.h"
PLATFORM = ROOT / "my355" / "platform" / "platform.c"
BT = ROOT / "all" / "common" / "generic_bt.c"


def fail(msg):
    print("bt-status: " + msg, file=sys.stderr)
    return 1


def between(text, start, end):
    return text.split(start, 1)[1].split(end, 1)[0]


def main():
    api = API.read_text()
    header = API_H.read_text()
    platform = PLATFORM.read_text()
    bt = BT.read_text()
    draw = between(api, "int GFX_blitHardwareGroup", "int GFX_blitButtonGroup")
    if "int show_bt = PLAT_btIsConnected();" not in draw:
        return fail("the pill does not use the cached Bluetooth helper")
    if "BT_isConnected()" in draw:
        return fail("the pill still scans from the draw")
    if "GetAudioSink() != AUDIO_SINK_DEFAULT" not in draw:
        return fail("the audio glyph is no longer independent")
    if "#define BT_isConnected PLAT_bluetoothConnected" not in header:
        return fail("BT_isConnected no longer names the live check")
    if 'opendir("/sys/class/bluetooth")' not in bt:
        return fail("the live check no longer reads sysfs")
    helper = between(platform, "bool PLAT_btIsConnected(void)", "bool PLAT_supportsColorTemperature")
    if "return BT_enabled() && bluetoothConnected;" not in helper:
        return fail("Bluetooth off does not force the rune off")
    status = between(platform, "void PLAT_getNetworkStatus", "static void psy_read")
    if "bluetoothConnected = PLAT_bluetoothConnected();" not in status:
        return fail("the poll does not refresh the cache")
    if "bluetoothConnected = false;" not in status:
        return fail("a disabled radio does not clear the cache")
    if "static bool bluetoothConnected = false;" not in platform:
        return fail("the cache does not start disconnected")
    if "SDL_AtomicSet(&pwr.poll_network_status, 1);" not in api:
        return fail("the frontend poll is not started")
    if "SDL_AtomicSet(&pwr.update_secs, 5);" not in api:
        return fail("the poll interval is no longer five seconds")
    if "PLAT_getNetworkStatus(&is_online);" not in api:
        return fail("the background poll does not refresh network status")
    print("bt status draw ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
