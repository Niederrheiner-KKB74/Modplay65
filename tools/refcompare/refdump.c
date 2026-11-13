/* Step the micromod reference replay tick by tick and dump per-channel state. */
#include <stdio.h>
#include <stdlib.h>
#include "micromod.c" /* add micromod-c to the include path */

int main(int argc, char **argv) {
	FILE *f = fopen(argv[1], "rb");
	long len, i, n = 0, maxticks = argc > 2 ? atol(argv[2]) : 2000000;
	signed char *data;
	fseek(f, 0, SEEK_END); len = ftell(f); rewind(f);
	data = malloc(len + 65536);
	if (fread(data, 1, len, f) != (size_t)len) return 2;
	if (micromod_initialise(data, 48000) != 0) return 1;
	while (n < maxticks) {
		long end = sequence_tick();
		if (end && n > 0) break;
		printf("%ld %ld", pattern, row);
		for (i = 0; i < 4; i++) {
			struct channel *c = &channels[i];
			long period = c->period + c->vibrato_add, volume;
			period = period * arp_tuning[c->arpeggio_add] >> 11;
			period = (period >> 1) + (period & 1);
			volume = c->volume + c->tremolo_add;
			if (volume > 64) volume = 64;
			if (volume < 0) volume = 0;
			printf(" %ld %ld %ld", c->period ? period : 0, volume, (long)(c->sample_idx >> FP_SHIFT));
		}
		printf("\n");
		n++;
	}
	return 0;
}
