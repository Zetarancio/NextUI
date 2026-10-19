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

/* Always writes a terminator. A source longer than dst_len-1 is cut. */
static inline void bt_copy_term(char *dst, size_t dst_len, const char *src)
{
	size_t i;

	if (!dst || dst_len == 0)
		return;
	if (!src)
		src = "";
	for (i = 0; i + 1 < dst_len && src[i] != '\0'; i++)
		dst[i] = src[i];
	dst[i] = '\0';
}

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
/* Another client's Discovering property is not our session.
 * Call StartDiscovery unless this connection already holds one.
 * InProgress means BlueZ already recorded this client. */
/* my355 keeps zlyme-bluetooth's bluetoothctl agent. Settings does not
 * export Agent1: GDBus method calls need a running GMainContext, and
 * Settings does not iterate one. */
static inline int bt_settings_registers_agent(const char *platform)
{
	return platform != NULL && strcmp(platform, "my355") != 0;
}

/* Discovery runs only while the Bluetooth page is the active menu. */
static inline int bt_page_should_poll(int page_active)
{
	return page_active != 0;
}

static inline int bt_page_should_acquire(int page_active, int radio_on, int session_held)
{
	return page_active && radio_on && !session_held;
}

static inline int bt_page_should_release(int page_active, int session_held)
{
	return !page_active && session_held;
}

static inline int bt_kind_is_audio(int kind)
{
	return kind == 1;
}

static inline int bt_disc_needs_call(int session_held)
{
	return !session_held;
}

static inline int bt_session_after_call(int session_held, int powered, int result)
{
	if (session_held)
		return 1;
	if (!powered)
		return 0;
	if (result == BT_DISC_OK || result == BT_DISC_IN_PROGRESS)
		return 1;
	return 0;
}

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
	int connected;
};

static inline int bt_seen_from_fields(struct bt_seen *out, const char *addr,
				      const char *name, const char *alias,
				      const char *icon, unsigned int class_val,
				      int paired, int connected)
{
	char mac[18];
	char label[249];
	int kind;

	if (!out)
		return 0;
	memset(out, 0, sizeof(*out));
	bt_copy_term(mac, sizeof(mac), addr);
	if (!bt_mac_ok(mac))
		return 0;
	bt_copy_term(out->addr, sizeof(out->addr), mac);
	if (name && name[0])
		bt_copy_term(label, sizeof(label), name);
	else if (alias && alias[0])
		bt_copy_term(label, sizeof(label), alias);
	else
		bt_copy_term(label, sizeof(label), mac);
	bt_copy_term(out->name, sizeof(out->name), label);
	kind = bt_kind_from_class(class_val);
	if (!kind)
		kind = bt_kind_from_icon(icon);
	out->kind = kind;
	out->paired = paired ? 1 : 0;
	out->connected = connected ? 1 : 0;
	return 1;
}

static inline int bt_paired_row_same(const char *a_name, int a_conn, int a_bonded,
				     const char *b_name, int b_conn, int b_bonded)
{
	if (!a_name || !b_name)
		return 0;
	return strcmp(a_name, b_name) == 0 && a_conn == b_conn && a_bonded == b_bonded;
}

/* One GetManagedObjects snapshot feeds both rows. A paired device is
 * not also a scan row. An unpaired device is not a known row. */
static inline int bt_inventory_is_paired(const struct bt_seen *row)
{
	return row && row->paired && bt_mac_ok(row->addr);
}

static inline int bt_inventory_is_new(const struct bt_seen *row)
{
	return row && !row->paired && bt_mac_ok(row->addr);
}

#endif
