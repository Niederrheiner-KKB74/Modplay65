/*
 * modtrace - run the replay engine over a MOD file on the PC and print a
 * summary (and with -v every row). A development aid, not a unit test.
 *
 *   modtrace [-v] file.mod
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
    const char *name = argv[argc - 1];
    FILE *f;
    double seconds = 0;
    long ticks = 0, triggers = 0;
    unsigned effects_seen = 0;
    int i;

    if (argc < 2 || !(f = fopen(name, "rb"))) {
        fprintf(stderr, "usage: modtrace [-v] file.mod\n");
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
    mod_player_init(&p, &info, 0);
    while (!p.song_looped && ticks < 2000000) {
        uint8_t order = p.order, row = p.row, pattern = p.pattern;

        mod_player_tick(&p);
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
