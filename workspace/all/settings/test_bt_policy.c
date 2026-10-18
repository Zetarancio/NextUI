#include "../common/bt_policy.h"
#include <stdio.h>

static int fails;

static void check(int cond, const char *msg)
{
	if (!cond) {
		fprintf(stderr, "bt policy: %s\n", msg);
		fails = 1;
	}
}

int main(void)
{
	check(bt_mac_ok("AA:BB:CC:DD:EE:FF"), "upper mac");
	check(bt_mac_ok("aa:bb:cc:dd:ee:ff"), "lower mac");
	check(!bt_mac_ok("AA:BB:CC:DD:EE"), "short mac");
	check(!bt_mac_ok("AA:BB:CC:DD:EE:FF:00"), "long mac");
	check(!bt_mac_ok("AABBCCDDEEFF"), "no colons");
	check(!bt_mac_ok("GG:BB:CC:DD:EE:FF"), "bad digit");
	check(bt_kind_from_class(0x240404) == 1, "audio class");
	check(bt_kind_from_class(0x000508) == 2, "controller class");
	check(bt_kind_from_class(0) == 0, "missing class");
	check(bt_kind_from_icon("audio-headset") == 1, "audio icon");
	check(bt_kind_from_icon("input-gaming") == 2, "pad icon");
	check(bt_kind_from_icon(NULL) == 0, "no icon");
	check(bt_kind_from_icon("unknown") == 0, "neutral icon class");
	check(bt_disc_classify(0, NULL) == BT_DISC_OFF, "powered off");
	check(bt_disc_classify(1, NULL) == BT_DISC_OK, "started");
	check(bt_disc_classify(1, "org.bluez.Error.InProgress") == BT_DISC_IN_PROGRESS, "in progress");
	check(bt_disc_classify(1, "org.bluez.Error.NotReady") == BT_DISC_NOT_READY, "not ready");
	check(bt_disc_classify(1, "org.freedesktop.DBus.Error.ServiceUnknown") == BT_DISC_GONE, "daemon gone");
	check(bt_disc_classify(1, "org.bluez.Error.Failed") == BT_DISC_FAILED, "failed");
	check(bt_disc_is_active(BT_DISC_OK, 0), "success is active");
	check(!bt_disc_is_active(BT_DISC_IN_PROGRESS, 0), "in progress without the property");
	check(bt_disc_is_active(BT_DISC_IN_PROGRESS, 1), "in progress with the property");
	check(!bt_disc_is_active(BT_DISC_FAILED, 1), "failed is not active");
	check(!bt_disc_is_active(BT_DISC_NOT_READY, 0), "not ready retries");
	check(bt_scan_row_same("AA:BB:CC:DD:EE:FF", "Pad", 2, "AA:BB:CC:DD:EE:FF", "Pad", 2), "same row");
	check(!bt_scan_row_same("AA:BB:CC:DD:EE:FF", "Pad", 2, "AA:BB:CC:DD:EE:FF", "Pad 2", 2), "rename redraws");
	check(!bt_scan_row_same("AA:BB:CC:DD:EE:FF", "Pad", 0, "AA:BB:CC:DD:EE:FF", "Pad", 2), "class change redraws");
	check(!bt_paired_row_same("Pad", 0, 1, "Pad", 1, 1), "connect redraws");
	if (fails)
		return 1;
	printf("bt policy ok\n");
	return 0;
}
