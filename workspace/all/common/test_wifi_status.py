#!/usr/bin/env python3
"""Source check for the my355 Wi-Fi pill publication.

The category behavior is test_wifi_publish.c. This file checks that
the draw does not grow a second scan.
"""
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
PLATFORM = ROOT / "my355" / "platform" / "platform.c"
API = ROOT / "all" / "common" / "api.c"
CONFIG = ROOT / "all" / "common" / "config.c"


def fail(msg):
    print("wifi-status: " + msg, file=sys.stderr)
    return 1


def between(text, start, end):
    return text.split(start, 1)[1].split(end, 1)[0]


def main():
    platform = PLATFORM.read_text()
    api = API.read_text()
    config = CONFIG.read_text()
    draw = between(api, "int GFX_blitHardwareGroup", "int GFX_blitButtonGroup")
    status = between(platform, "void PLAT_getNetworkStatus", "static void psy_read")
    strength = between(platform, "ConnectionStrength PLAT_connectionStrength", "int PLAT_setDateTime")
    if "static struct WIFI_connection connection" in platform:
        return fail("the connection struct is still shared")
    if "SDL_AtomicSet(&wifiStrength, zlyme_wifi_store(category));" not in status:
        return fail("the poll does not publish one category")
    if "struct WIFI_connection info" not in status:
        return fail("the scan result is not local to the poll")
    if "zlyme_wifi_visible(WIFI_enabled()," not in strength:
        return fail("the pill does not read one published category")
    if "connection.valid" in strength or "connection.rssi" in strength:
        return fail("the pill still reads connection fields")
    for needle in ("wpa_cli", "opendir(", "/sys/", "iw dev"):
        if needle in strength:
            return fail("the pill scans: " + needle)
    if "ConnectionStrength strength = PLAT_connectionStrength();" not in draw:
        return fail("the draw no longer uses the strength helper")
    if "WIFI_connectionInfo" in draw:
        return fail("the draw calls the live Wi-Fi scan")
    if "__atomic_load_n(&settings.wifi, __ATOMIC_RELAXED)" not in config:
        return fail("the wifi flag is read without an atomic load")
    if "__atomic_store_n(&settings.wifi, on, __ATOMIC_RELAXED)" not in config:
        return fail("the wifi flag is written without an atomic store")
    if 'fprintf(file, "wifi=%i\\n", (int)wifi_load());' not in config:
        return fail("the settings file does not keep wifi=%i")
    if 'fprintf(file, "wifi=%i\\n", settings.wifi)' in config:
        return fail("the settings file reads the flag unsynchronized")
    if "SDL_AtomicSet(&pwr.update_secs, 5);" not in api:
        return fail("the poll interval is no longer five seconds")
    print("wifi status ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
