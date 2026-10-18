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
	{
		char buf[18];
		char name[249];
		struct bt_seen row;
		unsigned char dirty[18];
		int i;
		const char *mac = "AA:BB:CC:DD:EE:FF";
		char huge[400];

		memset(dirty, 0xA5, sizeof(dirty));
		memcpy(dirty, mac, 17);
		bt_copy_term(buf, sizeof(buf), (char *)dirty);
		check(strlen(buf) == 17, "exact mac length");
		check(buf[17] == '\0', "mac terminator");
		check(bt_mac_ok(buf), "copied mac");
		check(!bt_seen_from_fields(&row, NULL, "x", NULL, NULL, 0, 0, 0), "missing mac");
		check(!bt_seen_from_fields(&row, "GG:BB:CC:DD:EE:FF", NULL, NULL, NULL, 0, 0, 0), "malformed mac");
		check(bt_seen_from_fields(&row, "AA:BB:CC:DD:EE:FF:00", NULL, NULL, NULL, 0, 0, 0), "oversized address copies");
		check(strlen(row.addr) == 17 && row.addr[17] == '\0', "oversized address stays bounded");
		memset(huge, 'N', sizeof(huge) - 1);
		huge[sizeof(huge) - 1] = '\0';
		check(bt_seen_from_fields(&row, mac, huge, NULL, NULL, 0, 0, 0), "long name accepted");
		check(strlen(row.name) == sizeof(name) - 1, "long name terminated");
		check(row.name[sizeof(row.name) - 1] == '\0', "name last byte");
		check(bt_seen_from_fields(&row, mac, NULL, NULL, NULL, 0, 0, 0), "unnamed device");
		check(strcmp(row.name, mac) == 0, "unnamed uses address");
		check(row.kind == 0, "missing icon and class");
		check(bt_disc_needs_call(0), "must acquire our own session");
		check(!bt_disc_needs_call(1), "do not acquire twice");
		check(bt_session_after_call(0, 1, BT_DISC_OK), "our start succeeded");
		check(bt_session_after_call(0, 1, BT_DISC_IN_PROGRESS), "this client already held it");
		check(!bt_session_after_call(0, 0, BT_DISC_OK), "powered off");
		check(!bt_session_after_call(0, 1, BT_DISC_FAILED), "failed start is not held");
		check(!bt_session_after_call(0, 1, BT_DISC_NOT_READY), "not ready retries later");
		(void)i;
	}
	if (fails)
		return 1;
	printf("bt policy ok\n");
	return 0;
}
