/* Host-side unit tests for src/mod/mod_play.c. */
#include <stdio.h>
#include <string.h>

#include "mod/mod_play.h"
#include "mod/mod_tables.h"

static int failures;

#define CHECK(cond)                                                    \
    do {                                                               \
        if (!(cond)) {                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);     \
            ++failures;                                                \
        }                                                              \
    } while (0)

static uint8_t patterns[4][MOD_ROWS][MOD_ROW_BYTES];
static struct mod_info info;
static struct mod_player p;

void mod_read_row(uint8_t pattern, uint8_t row, uint8_t *dst)
{
    memcpy(dst, patterns[pattern], 0); /* keep the compiler quiet */
    memcpy(dst, patterns[pattern][row], MOD_ROW_BYTES);
}

static void cell(int pat, int row, int ch, int sample, int period, int effect,
                 int param)
{
    uint8_t *d = patterns[pat][row] + 4 * ch;

    d[0] = (uint8_t)((sample & 0xF0) | (period >> 8));
    d[1] = (uint8_t)period;
    d[2] = (uint8_t)(((sample & 0x0F) << 4) | effect);
    d[3] = (uint8_t)param;
}

static void reset(void)
{
    memset(patterns, 0, sizeof patterns);
    memset(&info, 0, sizeof info);
    info.num_samples = 31;
    info.song_length = 3;
    info.order[0] = 0;
    info.order[1] = 1;
    info.order[2] = 2;
    info.sample[0].length = 1000;
    info.sample[0].volume = 64;
    info.sample[1].length = 5000;
    info.sample[1].volume = 40;
    info.sample[1].finetune = 3;
}

static void ticks(int n)
{
    while (n--) {
        mod_player_tick(&p);
    }
}

static void test_basic_note_and_timing(void)
{
    reset();
    cell(0, 0, 0, 1, 428, 0, 0);
    cell(0, 1, 1, 2, 428, 0xC, 20);
    mod_player_init(&p, &info, 0);

    mod_player_tick(&p);
    CHECK(p.row_started);
    CHECK(p.ch[0].out_flags & MOD_OUT_TRIGGER);
    CHECK(p.ch[0].out_sample == 1);
    CHECK(p.ch[0].out_period == 428);
    CHECK(p.ch[0].out_volume == 64);
    CHECK(!(p.ch[1].out_flags & MOD_OUT_TRIGGER));
    ticks(5);
    CHECK(p.row == 1 && p.tick == 0); /* speed 6 */
    CHECK(!(p.ch[0].out_flags & MOD_OUT_TRIGGER));
    mod_player_tick(&p);
    CHECK(p.ch[1].out_flags & MOD_OUT_TRIGGER);
    CHECK(p.ch[1].out_volume == 20);                      /* Cxx      */
    CHECK(p.ch[1].out_period == mod_period_table[3][12]); /* finetune */
    CHECK(p.ch[1].out_period < 428);
}

static void test_speed_break_jump(void)
{
    reset();
    cell(0, 0, 0, 0, 0, 0xF, 3);    /* speed 3            */
    cell(0, 1, 0, 0, 0, 0xF, 150);  /* tempo 150          */
    cell(0, 2, 0, 0, 0, 0xD, 0x12); /* break to row 12    */
    cell(1, 12, 0, 0, 0, 0xB, 2);   /* jump to position 2 */
    cell(2, 0, 0, 0, 0, 0xB, 0);    /* back to the start  */
    mod_player_init(&p, &info, 0);
    ticks(3);
    CHECK(p.speed == 3 && p.row == 1);
    ticks(3);
    CHECK(p.tempo == 150 && p.row == 2);
    ticks(3);
    CHECK(p.order == 1 && p.pattern == 1 && p.row == 12);
    ticks(3);
    CHECK(p.order == 2 && p.row == 0);
    CHECK(!p.song_looped);
    ticks(3);
    CHECK(p.order == 0 && p.row == 0);
    CHECK(p.song_looped);
}

static void test_slides(void)
{
    reset();
    cell(0, 0, 0, 1, 428, 0x1, 4);  /* slide up 4 per tick     */
    cell(0, 1, 0, 0, 0, 0xA, 0x02); /* volume down 2 per tick  */
    cell(0, 2, 0, 0, 214, 0x3, 50); /* tone portamento to 214  */
    cell(0, 3, 0, 0, 0, 0x3, 0);
    mod_player_init(&p, &info, 0);
    ticks(6);
    CHECK(p.ch[0].out_period == 428 - 5 * 4);
    ticks(6);
    CHECK(p.ch[0].out_volume == 64 - 5 * 2);
    mod_player_tick(&p);
    CHECK(!(p.ch[0].out_flags & MOD_OUT_TRIGGER)); /* 3xx does not retrigger */
    ticks(5);
    CHECK(p.ch[0].out_period == 214); /* 408 - 5*50 would overshoot */
    ticks(6);
    CHECK(p.ch[0].out_period == 214);
}

static void test_arpeggio_vibrato_offset(void)
{
    reset();
    cell(0, 0, 0, 1, 428, 0x0, 0x47);
    cell(0, 1, 0, 1, 428, 0x4, 0x8F);
    cell(0, 2, 0, 2, 428, 0x9, 0x02);
    cell(0, 3, 0, 2, 428, 0x9, 0xFF); /* beyond the sample end */
    mod_player_init(&p, &info, 0);
    mod_player_tick(&p);
    CHECK(p.ch[0].out_period == 428);
    mod_player_tick(&p);
    CHECK(p.ch[0].out_period == mod_period_table[0][12 + 4]);
    mod_player_tick(&p);
    CHECK(p.ch[0].out_period == mod_period_table[0][12 + 7]);
    mod_player_tick(&p);
    CHECK(p.ch[0].out_period == 428);
    ticks(2);
    mod_player_tick(&p); /* row 1, tick 0 */
    CHECK(p.ch[0].out_period == 428);
    mod_player_tick(&p);
    CHECK(p.ch[0].out_period != 428); /* vibrato moves the pitch ... */
    CHECK(p.ch[0].period == 428);     /* ... but not the base period */
    ticks(4);
    mod_player_tick(&p); /* row 2 */
    CHECK(p.ch[0].out_flags & MOD_OUT_TRIGGER);
    CHECK(p.ch[0].out_offset == 0x200);
    ticks(5);
    mod_player_tick(&p); /* row 3 */
    CHECK(!(p.ch[0].out_flags & MOD_OUT_TRIGGER));
    CHECK(p.ch[0].out_period == 0);
}

static void test_extended(void)
{
    reset();
    cell(0, 0, 0, 1, 428, 0xE, 0xD3); /* note delay 3      */
    cell(0, 1, 0, 0, 0, 0xE, 0xC2);   /* cut at tick 2     */
    cell(0, 2, 0, 1, 428, 0xE, 0x92); /* retrigger every 2 */
    cell(0, 3, 0, 0, 0, 0xE, 0x60);   /* loop start        */
    cell(0, 4, 0, 0, 0, 0xE, 0x62);   /* loop twice        */
    cell(0, 5, 0, 0, 0, 0xE, 0xE2);   /* pattern delay 2   */
    mod_player_init(&p, &info, 0);
    ticks(3);
    CHECK(!(p.ch[0].out_flags & MOD_OUT_TRIGGER));
    mod_player_tick(&p);
    CHECK(p.ch[0].out_flags & MOD_OUT_TRIGGER);
    ticks(2);
    ticks(2); /* row 1, ticks 0 and 1 */
    CHECK(p.ch[0].out_volume == 64);
    mod_player_tick(&p);
    CHECK(p.ch[0].out_volume == 0);
    ticks(3);
    mod_player_tick(&p); /* row 2 tick 0 */
    CHECK(p.ch[0].out_flags & MOD_OUT_TRIGGER);
    mod_player_tick(&p);
    CHECK(!(p.ch[0].out_flags & MOD_OUT_TRIGGER));
    mod_player_tick(&p);
    CHECK(p.ch[0].out_flags & MOD_OUT_TRIGGER);
    ticks(3);
    CHECK(p.row == 3);
    ticks(12);
    CHECK(p.row == 3); /* first loop back  */
    ticks(12);
    CHECK(p.row == 3); /* second loop back */
    ticks(12);
    CHECK(p.row == 5); /* loop finished    */
    ticks(6);
    CHECK(p.row == 5); /* pattern delay    */
    ticks(12);
    CHECK(p.row == 6);
}

int main(void)
{
    test_basic_note_and_timing();
    test_speed_break_jump();
    test_slides();
    test_arpeggio_vibrato_offset();
    test_extended();
    if (failures) {
        printf("%d check(s) failed\n", failures);
        return 1;
    }
    printf("all tests passed\n");
    return 0;
}
