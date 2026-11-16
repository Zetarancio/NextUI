/* Host check for the my355 Wi-Fi category publisher.
 * The classifier is the header compiled into platform.c.
 * The threads publish one atomic category. They do not run wpa_cli. */
#include <SDL2/SDL_atomic.h>
#include <pthread.h>
#include <stdio.h>

#include "../../my355/platform/wifi_publish.h"

static SDL_atomic_t published;
static SDL_atomic_t wifi_on;
static int failed;

static void expect(int cond, const char *msg)
{
	if (!cond) {
		fprintf(stderr, "wifi-publish: %s\n", msg);
		failed = 1;
	}
}

static void check_table(void)
{
	expect(zlyme_wifi_category(0, -40) == -1, "unavailable");
	expect(zlyme_wifi_category(1, -1) == -1, "rssi -1");
	expect(zlyme_wifi_category(1, 0) == 0, "rssi 0");
	expect(zlyme_wifi_category(1, -59) == 3, "rssi -59");
	expect(zlyme_wifi_category(1, -60) == 3, "rssi -60");
	expect(zlyme_wifi_category(1, -61) == 2, "rssi -61");
	expect(zlyme_wifi_category(1, -70) == 2, "rssi -70");
	expect(zlyme_wifi_category(1, -71) == 1, "rssi -71");
	expect(zlyme_wifi_load(0) == -1, "initial atomic is off");
	expect(zlyme_wifi_visible(0, zlyme_wifi_store(3)) == -1, "disabled hides high");
	expect(zlyme_wifi_visible(1, 0) == -1, "enabled before the first poll");
}

static void check_roundtrip(void)
{
	int i;
	int cats[5] = {-1, 0, 1, 2, 3};

	for (i = 0; i < 1000; i++) {
		int cat = cats[i % 5];
		int stored = zlyme_wifi_store(cat);
		expect(zlyme_wifi_load(stored) == cat, "round trip");
		expect(zlyme_wifi_visible(1, stored) == cat, "visible category");
	}
}

static void *writer(void *arg)
{
	int i;
	int cats[5] = {-1, 0, 3, 0, 2};
	(void)arg;
	for (i = 0; i < 200000; i++) {
		int on = (i & 8) != 0;
		int cat = cats[i % 5];
		SDL_AtomicSet(&wifi_on, on);
		SDL_AtomicSet(&published, zlyme_wifi_store(on ? cat : -1));
	}
	return NULL;
}

static int legal(int cat)
{
	return cat == -1 || cat == 0 || cat == 1 || cat == 2 || cat == 3;
}

static void *reader(void *arg)
{
	int i;
	unsigned long *samples = arg;

	*samples = 0;
	for (i = 0; i < 200000; i++) {
		int on = SDL_AtomicGet(&wifi_on);
		int stored = SDL_AtomicGet(&published);
		int shown = zlyme_wifi_visible(on, stored);

		if (!legal(zlyme_wifi_load(stored)) || !legal(shown)) {
			failed = 1;
			return NULL;
		}
		(*samples)++;
	}
	return NULL;
}

int main(void)
{
	pthread_t w, r;
	unsigned long samples = 0;
	SDL_atomic_t initial = {0};

	check_table();
	check_roundtrip();
	expect(zlyme_wifi_visible(1, SDL_AtomicGet(&initial)) == -1,
		"zero-initialized atomic");
	if (pthread_create(&w, NULL, writer, NULL) != 0)
		return 1;
	if (pthread_create(&r, NULL, reader, &samples) != 0)
		return 1;
	pthread_join(w, NULL);
	pthread_join(r, NULL);
	if (failed || samples == 0)
		return 1;
	printf("wifi publish ok\n");
	return 0;
}
