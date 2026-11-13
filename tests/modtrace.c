/*
 * modtrace - run the replay engine over a MOD file on the PC and print a
 * summary (and with -v every row). A development aid, not a unit test.
 *
 *   modtrace [-v|-t] file.mod
 *
 * -t prints one line per tick: "order row", then period, volume and a
 * trigger marker (sample offset or -1) for each channel, then the effects.
 * Used to compare the engine with a reference player.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mod/mod_play.h"

static uint8_t *file;
static long file_size;
static struct mod_info info;
static struct mod_player p;

void mod_read_row(uint8_t pattern, uint8_t row, uint8_t *dst)
{
    long off = (long)info.pattern_offset + (long)pattern * 1024 + row * 16;

    if (off + 16 <= file_size) {
        memcpy(dst, file + off, 16);
    } else {
        memset(dst, 0, 16);
    }
}

int main(int argc, char **argv)
{
    int verbose = argc > 2 && !strcmp(argv[1], "-v");
    int per_tick = argc > 2 && !strcmp(argv[1], "-t");
    const char *name = argv[argc - 1];
    FILE *f;
    double seconds = 0;
    long ticks = 0, triggers = 0;
    unsigned effects_seen = 0;
    int i;

    if (argc < 2 || !(f = fopen(name, "rb"))) {
        fprintf(stderr, "usage: modtrace [-v|-t] file.mod\n");
        return 2;
    }
    fseek(f, 0, SEEK_END);
    file_size = ftell(f);
    rewind(f);
    file = malloc((size_t)file_size + 1);
    if (fread(file, 1, (size_t)file_size, f) != (size_t)file_size) {
        return 2;
    }
    fclose(f);
    i = mod_parse_header(file, file_size < 1084 ? (uint16_t)file_size : 1084,
                         &info);
    if (i != MOD_OK) {
        printf("parse error %d\n", i);
        return 1;
    }
    {
        /* same sample memory as the MEGA65 build (mod_file.h) */
        static const struct mod_region regions[] = {
            { 0x040000UL, 0x04FFFFUL },
            { 0x050000UL, 0x05FFFFUL },
            { 0x012000UL, 0x01F800UL }
        };
        uint32_t addr[MOD_MAX_SAMPLES];
        uint8_t shift[MOD_MAX_SAMPLES];
        uint8_t reduced;

        i = mod_fit_samples(&info, regions, 3, addr, shift, &reduced);
        if (!per_tick) {
            printf("sample memory: %lu bytes, %s", (unsigned long)info.sample_bytes,
                   i != MOD_OK ? "DOES NOT FIT" : reduced ? "fits with reduced samples:" : "fits");
            for (i = 0; i < MOD_MAX_SAMPLES; ++i) {
                if (shift[i]) {
                    printf(" #%d (%lu bytes) 1/%d", i + 1,
                           (unsigned long)info.sample[i].length, 1 << shift[i]);
                }
            }
            printf("\n");
        }
    }
    mod_player_init(&p, &info, 0);
    while (!p.song_looped && ticks < 2000000) {
        uint8_t order = p.order, row = p.row, pattern = p.pattern;

        mod_player_tick(&p);
        if (per_tick) {
            printf("%d %d", order, row);
            for (i = 0; i < MOD_CHANNELS; ++i) {
                printf(" %d %d %d", p.ch[i].out_period, p.ch[i].out_volume,
                       p.ch[i].out_flags & MOD_OUT_TRIGGER
                           ? (int)p.ch[i].out_offset : -1);
            }
            printf(" |");
            for (i = 0; i < MOD_CHANNELS; ++i) {
                printf(" %X%02X", p.ch[i].effect, p.ch[i].param);
            }
            printf("\n");
        }
        seconds += 2.5 / p.tempo;
        ++ticks;
        for (i = 0; i < MOD_CHANNELS; ++i) {
            if (p.ch[i].out_flags & MOD_OUT_TRIGGER) {
                ++triggers;
            }
            if (p.row_started && (p.ch[i].effect || p.ch[i].param)) {
                effects_seen |= 1u << p.ch[i].effect;
            }
        }
        if (verbose && p.row_started) {
            printf("%3d %2d/%02d |", order, pattern, row);
            for (i = 0; i < MOD_CHANNELS; ++i) {
                const struct mod_channel *c = &p.ch[i];

                printf(" %c s%02d p%4d v%2d %X%02X |",
                       c->out_flags & MOD_OUT_TRIGGER ? '*' : ' ', c->sample,
                       c->out_period, c->out_volume, c->effect, c->param);
            }
            printf("\n");
        }
    }
    if (per_tick) {
        return 0;
    }
    printf("\"%s\": %ld ticks, %.1f s, %ld note triggers, final speed %d tempo %d\n",
           info.name, ticks, seconds, triggers, p.speed, p.tempo);
    printf("effects used:");
    for (i = 0; i < 16; ++i) {
        if (effects_seen & (1u << i)) {
            printf(" %X", i);
        }
    }
    printf("\n");
    return 0;
}
