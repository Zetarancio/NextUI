#!/usr/bin/env python3
"""Source checks for the single my355 timezone row.

This does not draw the menu and does not read a timezone file.
"""
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
SETTINGS = ROOT / "all" / "settings" / "settings.cpp"
PLATFORM = ROOT / "my355" / "platform" / "platform.c"


def fail(msg):
    print("timezone-selector: " + msg, file=sys.stderr)
    return 1


def between(text, start, end):
    return text.split(start, 1)[1].split(end, 1)[0]


def main():
    settings = SETTINGS.read_text()
    platform = PLATFORM.read_text()
    for gone in ("class CommonZoneItem", "class TimezonePicker", "class ZlymeTimezoneRow",
                 '"All timezones"', '"Common"', "zlyme_common", "zlyme_all"):
        if gone in settings:
            return fail("the nested timezone picker is still present: " + gone)
    if settings.count('Generic, "Time zone"') != 2:
        return fail("my355 and the other platforms do not each have one Time zone row")
    row = between(settings, "deviceInfo.getPlatform() == DeviceInfo::my355) {", "else if (tz_count > 0)")
    if "new MenuItem" not in row:
        return fail("my355 does not use the ordinary menu row")
    if "timezone-representatives.txt" not in row:
        return fail("my355 does not read the representative list")
    if "zlyme_zone_pull()" not in row:
        return fail("my355 does not read the saved zone at startup")
    if row.count("TIME_getCurrentTimezone") != 0:
        return fail("the my355 row reads the saved zone itself")
    if "if (zone == zlyme_zone.label())" not in row:
        return fail("an unchanged zone is written again")
    if "TIME_setCurrentTimezone(zone.c_str())" not in row:
        return fail("a choice does not save the canonical name")
    if "shown_values.push_back(now)" not in row:
        return fail("a saved uncommon zone is not kept")
    if "zlyme_zone_reset" not in row:
        return fail("reset is not UTC")
    if settings.count("zlymeReadSavedZone") != 2:
        return fail("the saved zone is not read from one function")
    if "CFG_setCurrentTimezone" in settings:
        return fail("settings stores a timezone index itself")
    if 'TIME_setCurrentTimezone("UTC")' not in settings:
        return fail("reset is not UTC")
    other = between(settings, "else if (tz_count > 0)", "Haptic feedback")
    if "tz_values, tz_labels" not in other:
        return fail("other platforms lost the full timezone list")
    if "TIME_getCurrentTimezone" not in other:
        return fail("other platforms no longer read their own zone")
    if "qsort(cached_timezones" not in platform:
        return fail("the canonical list is no longer sorted in place")
    if "CFG_setCurrentTimezone(tz_index)" not in platform:
        return fail("the platform no longer stores the full-list index")
    if 'strncpy(output, "UTC"' not in platform:
        return fail("UTC is no longer the fallback")
    if "tz_index == -1" not in platform:
        return fail("an unknown zone is no longer rejected")
    print("timezone selector ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
