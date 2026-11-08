#!/usr/bin/env python3
"""The Settings timezone row keeps canonical names. It does not store a short-list index."""
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
SETTINGS = ROOT / "all" / "settings" / "settings.cpp"
PLATFORM = ROOT / "my355" / "platform" / "platform.c"


def fail(msg):
    print("timezone-selector: " + msg, file=sys.stderr)
    return 1


def main():
    settings = SETTINGS.read_text()
    platform = PLATFORM.read_text()
    if "ZlymeTimezoneRow" not in settings:
        return fail("the timezone row is gone")
    if 'new ZlymeTimezoneRow' not in settings:
        return fail("my355 does not open the timezone row")
    if '"All timezones"' not in settings or '"Common"' not in settings:
        return fail("common and full lists are not both present")
    if settings.count("TIME_setCurrentTimezone") < 3:
        return fail("choices are not saved by zone name")
    if "CFG_setCurrentTimezone" in settings:
        return fail("settings stores a timezone index itself")
    row = settings.split("class ZlymeTimezoneRow", 1)[1].split("int main", 1)[0]
    handler = row.split("handleInput", 1)[1]
    if "TIME_setCurrentTimezone" in handler:
        return fail("opening or moving on the row writes a timezone")
    if 'TIME_setCurrentTimezone("UTC")' not in row:
        return fail("reset is not UTC")
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
