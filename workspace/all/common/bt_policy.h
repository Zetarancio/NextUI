#ifndef NEXTUI_BT_POLICY_H
#define NEXTUI_BT_POLICY_H

/* Decisions shared by the Bluetooth settings page. No D-Bus here. */

#include <ctype.h>
#include <string.h>

enum {
	BT_DISC_OK = 0,
	BT_DISC_NOT_READY,
	BT_DISC_IN_PROGRESS,
	BT_DISC_FAILED,
	BT_DISC_OFF,
	BT_DISC_GONE
};

static inline int bt_mac_ok(const char *s)
{
	int i;

	if (!s || strlen(s) != 17)
		return 0;
	for (i = 0; i < 17; i++) {
		if ((i % 3) == 2) {
			if (s[i] != ':')
				return 0;
		} else if (!isxdigit((unsigned char)s[i])) {
			return 0;
		}
	}
	return 1;
}

/* BlueZ major device class. Unknown stays 0 and is still shown. */
static inline int bt_kind_from_class(unsigned int class_val)
{
	int major = (class_val & 0x1F00) >> 8;

	if (major == 0x04)
		return 1; /* audio */
	if (major == 0x05)
		return 2; /* peripheral / controller */
	return 0;
}

static inline int bt_kind_from_icon(const char *icon)
{
	if (!icon || !icon[0])
		return 0;
	if (strstr(icon, "audio") || strstr(icon, "headset") || strstr(icon, "headphone"))
		return 1;
	if (strstr(icon, "input-gaming") || strstr(icon, "input-keyboard") || strstr(icon, "input-mouse"))
		return 2;
	return 0;
}

static inline int bt_disc_classify(int powered, const char *err)
{
	if (!powered)
		return BT_DISC_OFF;
	if (!err || !err[0])
		return BT_DISC_OK;
	if (strstr(err, "InProgress"))
		return BT_DISC_IN_PROGRESS;
	if (strstr(err, "NotReady"))
		return BT_DISC_NOT_READY;
	if (strstr(err, "ServiceUnknown") || strstr(err, "NoReply") ||
	    strstr(err, "Disconnected") || strstr(err, "NameHasNoOwner"))
		return BT_DISC_GONE;
	return BT_DISC_FAILED;
}

/* Active only with a successful call, or InProgress while BlueZ agrees. */
static inline int bt_disc_is_active(int result, int property_discovering)
{
	if (result == BT_DISC_OK)
		return 1;
	if (result == BT_DISC_IN_PROGRESS && property_discovering)
		return 1;
	return 0;
}

static inline int bt_scan_row_same(const char *a_addr, const char *a_name, int a_kind,
				   const char *b_addr, const char *b_name, int b_kind)
{
	if (!a_addr || !b_addr || !a_name || !b_name)
		return 0;
	return strcmp(a_addr, b_addr) == 0 && strcmp(a_name, b_name) == 0 && a_kind == b_kind;
}

struct bt_seen {
	char addr[18];
	char name[249];
	int kind;
	int paired;
};

static inline int bt_paired_row_same(const char *a_name, int a_conn, int a_bonded,
				     const char *b_name, int b_conn, int b_bonded)
{
	if (!a_name || !b_name)
		return 0;
	return strcmp(a_name, b_name) == 0 && a_conn == b_conn && a_bonded == b_bonded;
}

#endif
