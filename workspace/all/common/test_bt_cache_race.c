/* Host threads for the SDL atomic and the relaxed bool used by the
 * my355 Bluetooth cache. This is not the Flip binary and it does not
 * open sysfs. Any interleaving is valid. The reader does not have to
 * observe both states. */
#include <SDL2/SDL_atomic.h>
#include <pthread.h>
#include <stdio.h>

static SDL_atomic_t connected;
static _Bool enabled;

struct observation {
	int invalid;
	unsigned long samples;
};

static void *writer(void *arg)
{
	int i;
	(void)arg;
	for (i = 0; i < 200000; i++) {
		SDL_AtomicSet(&connected, i & 1);
		__atomic_store_n(&enabled, (i & 1) != 0, __ATOMIC_RELAXED);
	}
	return NULL;
}

static void *reader(void *arg)
{
	struct observation *out = arg;
	int i;

	out->invalid = 0;
	out->samples = 0;
	for (i = 0; i < 200000; i++) {
		int link = SDL_AtomicGet(&connected);
		_Bool on = __atomic_load_n(&enabled, __ATOMIC_RELAXED);

		if (link != 0 && link != 1) {
			out->invalid = 1;
			return NULL;
		}
		if (on != 0 && on != 1) {
			out->invalid = 1;
			return NULL;
		}
		out->samples++;
	}
	return NULL;
}

int main(void)
{
	pthread_t w, r;
	struct observation obs = {0, 0};

	if (SDL_AtomicGet(&connected) != 0)
		return 1;
	if (pthread_create(&w, NULL, writer, NULL) != 0)
		return 1;
	if (pthread_create(&r, NULL, reader, &obs) != 0)
		return 1;
	pthread_join(w, NULL);
	pthread_join(r, NULL);
	if (obs.invalid || obs.samples == 0)
		return 1;
	if (SDL_AtomicGet(&connected) != 0 && SDL_AtomicGet(&connected) != 1)
		return 1;
	printf("bt cache race ok\n");
	return 0;
}
