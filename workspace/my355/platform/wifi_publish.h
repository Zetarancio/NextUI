/* Signal category published by the my355 network poll.
 * Values match ConnectionStrength: OFF -1, DISCONNECTED 0, LOW 1,
 * MED 2, HIGH 3. The poll stores category + 1, so a zero SDL atomic
 * is OFF. This file does not scan Wi-Fi. */
#ifndef WIFI_PUBLISH_H
#define WIFI_PUBLISH_H

static inline int zlyme_wifi_category(int valid, int rssi)
{
	if (!valid || rssi == -1)
		return -1;
	if (rssi == 0)
		return 0;
	if (rssi >= -60)
		return 3;
	if (rssi >= -70)
		return 2;
	return 1;
}

static inline int zlyme_wifi_store(int category)
{
	return category + 1;
}

static inline int zlyme_wifi_load(int stored)
{
	return stored - 1;
}

/* Wi-Fi off hides the symbol before the next poll. */
static inline int zlyme_wifi_visible(int enabled, int stored)
{
	if (!enabled)
		return -1;
	return zlyme_wifi_load(stored);
}

#endif
