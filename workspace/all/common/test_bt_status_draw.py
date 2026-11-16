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
    if "return BT_enabled() && SDL_AtomicGet(&bluetoothConnected);" not in helper:
        return fail("Bluetooth off does not force the rune off")
    status = between(platform, "void PLAT_getNetworkStatus", "static void psy_read")
    if "SDL_AtomicSet(&bluetoothConnected, PLAT_bluetoothConnected() ? 1 : 0);" not in status:
        return fail("the poll does not publish the cache")
    if "SDL_AtomicSet(&bluetoothConnected, 0);" not in status:
        return fail("a disabled radio does not clear the cache")
    if "static SDL_atomic_t bluetoothConnected;" not in platform:
        return fail("the cache is not an SDL atomic")
    if "static bool bluetoothConnected" in platform:
        return fail("the cache is still a plain bool")
    config = (ROOT / "all" / "common" / "config.c").read_text()
    if "__atomic_load_n(&settings.bluetooth, __ATOMIC_RELAXED)" not in config:
        return fail("the enable flag is read without an atomic load")
    if "__atomic_store_n(&settings.bluetooth, on, __ATOMIC_RELAXED)" not in config:
        return fail("the enable flag is written without an atomic store")
    if 'fprintf(file, "bluetooth=%i\\n", (int)bluetooth_load());' not in config:
        return fail("the settings file does not keep bluetooth=%i")
    if 'fprintf(file, "bluetooth=%i\\n", settings.bluetooth)' in config:
        return fail("the settings file reads the flag unsynchronized")
    for other in ("tg5040", "tg5050", "h700"):
        text = (ROOT / other / "platform" / "platform.c").read_text()
        body = between(text, "bool PLAT_btIsConnected(void)", "ConnectionStrength")
        if "return bluetoothConnected;" not in body:
            return fail(other + " no longer uses its own cache")
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
