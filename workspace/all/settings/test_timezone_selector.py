#!/usr/bin/env python3
"""Source checks for the Settings timezone row.

This does not draw the menu and does not read a timezone file. The
executable check of the in-memory name is test_timezone_state.cpp.
"""
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
SETTINGS = ROOT / "all" / "settings" / "settings.cpp"
HEADER = ROOT / "all" / "settings" / "zlymezone.hpp"
PLATFORM = ROOT / "my355" / "platform" / "platform.c"


def fail(msg):
    print("timezone-selector: " + msg, file=sys.stderr)
    return 1


def between(text, start, end):
    return text.split(start, 1)[1].split(end, 1)[0]


def main():
    settings = SETTINGS.read_text()
    header = HEADER.read_text()
    platform = PLATFORM.read_text()
    if "ZlymeTimezoneRow" not in settings:
        return fail("the timezone row is gone")
    if 'new ZlymeTimezoneRow' not in settings:
        return fail("my355 does not open the timezone row")
    if "class CommonZoneItem" not in settings or "class TimezonePicker" not in settings:
        return fail("the short list and the full list are not separate")
    if '"All timezones"' not in settings or '"Common"' not in settings:
        return fail("common and full lists are not both present")
    if "names.push_back(saved)" not in header or "all->reselect()" not in settings:
        return fail("a later uncommon choice is not kept on both lists")
    if settings.count("TIME_setCurrentTimezone") < 3:
        return fail("choices are not saved by zone name")
    if "CFG_setCurrentTimezone" in settings:
        return fail("settings stores a timezone index itself")
    if "zlyme_zone.label()" not in header and "const std::string &label()" not in header:
        return fail("the shown name is not an in-memory label")
    if "TIME_getCurrentTimezone" in header:
        return fail("the label helper reads the saved zone")

    common = between(settings, "class CommonZoneItem", "class TimezonePicker")
    draw = between(common, "const std::any getValue()", "InputReactionHint handleInput")
    if "TIME_getCurrentTimezone" in draw or "zlymeReadSavedZone" in draw or "zlyme_zone_pull" in draw:
        return fail("the short row reads the saved zone while drawing")
    row = between(settings, "class ZlymeTimezoneRow", "int main")
    row_draw = between(row, "const std::any getValue()", "InputReactionHint handleInput")
    if "TIME_getCurrentTimezone" in row_draw or "zlymeReadSavedZone" in row_draw or "zlyme_zone_pull" in row_draw:
        return fail("the parent row reads the saved zone while drawing")
    handler = row.split("handleInput", 1)[1]
    if "TIME_setCurrentTimezone" in handler or "zlyme_zone_remember" in handler:
        return fail("opening or moving on the row writes a timezone")
    if "zlyme_zone_reset" not in row:
        return fail("reset is not the shared UTC callback")
    picker = between(settings, "class TimezonePicker", "class ZlymeTimezoneRow")
    if "void onShow() override { zlyme_zone_pull(); }" not in picker:
        return fail("opening the picker does not refresh the saved name")
    showed, _, later = picker.partition("handleInput")
    if "zlyme_zone_pull" not in showed or "zlyme_zone_pull" in later or "TIME_getCurrentTimezone" in later:
        return fail("input on the picker reads the saved zone")
    calls = settings.split("static void zlyme_zone_pull();", 1)[1].count("zlyme_zone_pull();")
    if calls != 2:
        return fail("the saved zone is not read only at startup and when the picker opens")
    if "zlymeReadSavedZone" in between(settings, "static void zlyme_zone_remember", "static void zlyme_zone_pull"):
        return fail("a choice rereads the saved zone")
    if 'TIME_setCurrentTimezone("UTC")' not in settings:
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
