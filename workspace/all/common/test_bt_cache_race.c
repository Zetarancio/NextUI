/* Two threads publish and read the same flag shapes as the my355 cache
 * and the Bluetooth enable bit. This is not the Flip binary. */
#include <SDL2/SDL_atomic.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>

static SDL_atomic_t connected;
static _Bool enabled;

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
	int i;
	int seen = 0;
	(void)arg;
	for (i = 0; i < 200000; i++) {
		int link = SDL_AtomicGet(&connected);
		_Bool on = __atomic_load_n(&enabled, __ATOMIC_RELAXED);
		if (link != 0 && link != 1)
			return (void *)1;
		seen |= on ? 2 : 1;
	}
	return (void *)(intptr_t)seen;
}

int main(void)
{
	pthread_t w, r;
	void *got = NULL;
	if (SDL_AtomicGet(&connected) != 0)
		return 1;
	if (pthread_create(&w, NULL, writer, NULL) != 0)
		return 1;
	if (pthread_create(&r, NULL, reader, NULL) != 0)
		return 1;
	pthread_join(w, NULL);
	pthread_join(r, &got);
	if (got == (void *)1)
		return 1;
	if (SDL_AtomicGet(&connected) != 0 && SDL_AtomicGet(&connected) != 1)
		return 1;
	printf("bt cache race ok\n");
	return 0;
}
