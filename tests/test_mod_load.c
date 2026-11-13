/* Host-side unit tests for src/mod/mod_load.c. */
#include <stdio.h>
#include <string.h>

#include "mod/mod_load.h"

static int failures;

#define CHECK(cond)                                                    \
    do {                                                               \
        if (!(cond)) {                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);     \
            ++failures;                                                \
        }                                                              \
    } while (0)

static void put_sample(uint8_t *h, int n, const char *name, unsigned words,
                       unsigned finetune, unsigned vol, unsigned loop_start,
                       unsigned loop_words)
{
    uint8_t *s = h + 20 + 30 * n;

    strncpy((char *)s, name, 22);
    s[22] = (uint8_t)(words >> 8);
    s[23] = (uint8_t)words;
    s[24] = (uint8_t)finetune;
    s[25] = (uint8_t)vol;
    s[26] = (uint8_t)(loop_start >> 8);
    s[27] = (uint8_t)loop_start;
    s[28] = (uint8_t)(loop_words >> 8);
    s[29] = (uint8_t)loop_words;
}

static void make_mk(uint8_t *h)
{
    memset(h, 0, MOD_HEADER_SIZE);
    strcpy((char *)h, "test song");
    put_sample(h, 0, "lead", 4700, 0, 64, 0, 1);      /* no loop        */
    put_sample(h, 1, "pad", 1000, 15, 40, 100, 400);  /* loop, tune -1  */
    put_sample(h, 2, "broken", 50, 8, 99, 40, 100);   /* loop too long  */
    h[950] = 3;
    h[952] = 0;
    h[953] = 5;
    h[954] = 2;
    memcpy(h + 1080, "M.K.", 4);
}

static void test_mk(void)
{
    static uint8_t h[MOD_HEADER_SIZE];
    static struct mod_info info;

    make_mk(h);
    CHECK(mod_parse_header(h, sizeof h, &info) == MOD_OK);
    CHECK(strcmp(info.name, "test song") == 0);
    CHECK(strcmp(info.signature, "M.K.") == 0);
    CHECK(info.num_samples == 31);
    CHECK(info.song_length == 3);
    CHECK(info.num_patterns == 6);
    CHECK(info.pattern_offset == 1084);
    CHECK(info.sample_offset == 1084 + 6 * 1024);
    CHECK(info.sample[0].length == 9400);
    CHECK(info.sample[0].loop_length == 0);
    CHECK(info.sample[0].volume == 64);
    CHECK(strcmp(info.sample[0].name, "lead") == 0);
    CHECK(info.sample[1].length == 2000);
    CHECK(info.sample[1].loop_start == 200);
    CHECK(info.sample[1].loop_length == 800);
    CHECK(info.sample[1].finetune == -1);
    CHECK(info.sample[2].finetune == -8);
    CHECK(info.sample[2].volume == 64);
    CHECK(info.sample[2].loop_start == 80);
    CHECK(info.sample[2].loop_length == 20);
    CHECK(info.sample_bytes == 9400 + 2000 + 100);
}

static void test_other_formats(void)
{
    static uint8_t h[MOD_HEADER_SIZE];
    static struct mod_info info;

    make_mk(h);
    memcpy(h + 1080, "8CHN", 4);
    CHECK(mod_parse_header(h, sizeof h, &info) == MOD_ERR_CHANNELS);
    memcpy(h + 1080, "16CH", 4);
    CHECK(mod_parse_header(h, sizeof h, &info) == MOD_ERR_CHANNELS);
    memcpy(h + 1080, "4CHN", 4);
    CHECK(mod_parse_header(h, sizeof h, &info) == MOD_OK);

    memset(h, 0xFF, sizeof h);
    CHECK(mod_parse_header(h, sizeof h, &info) == MOD_ERR_NOT_A_MOD);
    CHECK(mod_parse_header(h, 100, &info) == MOD_ERR_TOO_SHORT);

    /* 15-instrument file: no signature, loop start in bytes */
    memset(h, 0, sizeof h);
    strcpy((char *)h, "old one");
    put_sample(h, 0, "bass", 500, 0, 64, 300, 100);
    h[470] = 2;
    h[472] = 1;
    h[473] = 0;
    CHECK(mod_parse_header(h, sizeof h, &info) == MOD_OK);
    CHECK(info.num_samples == 15);
    CHECK(info.signature[0] == 0);
    CHECK(info.num_patterns == 2);
    CHECK(info.pattern_offset == 600);
    CHECK(info.sample[0].loop_start == 300);
    CHECK(info.sample[0].loop_length == 200);
}

static void test_placement(void)
{
    static const struct mod_region two[] = {
        { 0x40000, 0x4FFFF }, { 0x50000, 0x5FFFF }
    };
    static const struct mod_region three[] = {
        { 0x40000, 0x4FFFF }, { 0x50000, 0x5FFFF }, { 0x12000, 0x1F800 }
    };
    static struct mod_info info;
    uint32_t addr[MOD_MAX_SAMPLES];

    /* largest first: 40000 into bank 4, 30000 does not fit behind it */
    memset(&info, 0, sizeof info);
    info.sample[0].length = 100;
    info.sample[2].length = 30000;
    info.sample[3].length = 40000;
    CHECK(mod_place_samples(&info, two, 2, addr) == MOD_OK);
    CHECK(addr[3] == 0x40000);
    CHECK(addr[2] == 0x50000);
    CHECK(addr[0] == 0x40000 + 40000);
    CHECK(addr[1] == 0);
    CHECK(mod_place_samples(&info, two, 1, addr) == MOD_ERR_NO_MEMORY);

    /* a third, smaller region takes what is left over */
    info.sample[2].length = 15000;
    info.sample[5].length = 50000;
    info.sample[6].length = 50000;
    CHECK(mod_place_samples(&info, two, 2, addr) == MOD_ERR_NO_MEMORY);
    CHECK(mod_place_samples(&info, three, 3, addr) == MOD_OK);
    CHECK(addr[5] == 0x40000);
    CHECK(addr[6] == 0x50000);
    CHECK(addr[3] == 0x12000);
    CHECK(addr[2] == 0x40000 + 50000);

    info.sample[4].length = 0x10000;
    CHECK(mod_place_samples(&info, three, 3, addr) == MOD_ERR_SAMPLE_TOO_BIG);

    /* the last byte of a bank stays unused */
    memset(&info, 0, sizeof info);
    info.sample[0].length = 0xFFFF;
    info.sample[1].length = 1;
    CHECK(mod_place_samples(&info, two, 2, addr) == MOD_OK);
    CHECK(addr[0] == 0x40000);
    CHECK(addr[1] == 0x50000);
}

static void test_fit(void)
{
    static const struct mod_region one[] = { { 0x40000, 0x4FFFF } };
    static struct mod_info info;
    uint32_t addr[MOD_MAX_SAMPLES];
    uint8_t shift[MOD_MAX_SAMPLES];
    uint8_t reduced;

    memset(&info, 0, sizeof info);
    info.sample[0].length = 30000;
    info.sample[1].length = 20000;
    CHECK(mod_fit_samples(&info, one, 1, addr, shift, &reduced) == MOD_OK);
    CHECK(reduced == 0 && shift[0] == 0 && shift[1] == 0);

    /* 60000 + 30000 + 20000 is too much: only the largest is halved */
    info.sample[2].length = 60000;
    CHECK(mod_fit_samples(&info, one, 1, addr, shift, &reduced) == MOD_ERR_NO_MEMORY ||
          reduced > 0);
    CHECK(mod_fit_samples(&info, one, 1, addr, shift, &reduced) == MOD_OK);
    CHECK(shift[2] >= 1);
    CHECK(shift[1] == 0);
    CHECK((60000u >> shift[2]) + (30000u >> shift[0]) + 20000u <= 0xFFFF);

    /* a sample larger than a bank becomes playable at half rate */
    memset(&info, 0, sizeof info);
    info.sample[0].length = 100000;
    CHECK(mod_place_samples(&info, one, 1, addr) == MOD_ERR_SAMPLE_TOO_BIG);
    CHECK(mod_fit_samples(&info, one, 1, addr, shift, &reduced) == MOD_OK);
    CHECK(shift[0] == 1 && reduced == 1 && addr[0] == 0x40000);

    /* hopeless: even a quarter of everything does not fit */
    for (reduced = 0; reduced < 31; ++reduced) {
        info.sample[reduced].length = 120000;
    }
    CHECK(mod_fit_samples(&info, one, 1, addr, shift, &reduced) != MOD_OK);
}

int main(void)
{
    test_fit();
    test_mk();
    test_other_formats();
    test_placement();
    if (failures) {
        printf("%d check(s) failed\n", failures);
        return 1;
    }
    printf("all tests passed\n");
    return 0;
}
