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
    static struct mod_info info;
    uint32_t addr[MOD_MAX_SAMPLES];

    memset(&info, 0, sizeof info);
    info.sample[0].length = 40000;
    info.sample[2].length = 30000; /* would cross into the next bank */
    info.sample[3].length = 100;
    CHECK(mod_place_samples(&info, 0x40000, 2, addr) == MOD_OK);
    CHECK(addr[0] == 0x40000);
    CHECK(addr[1] == 0);
    CHECK(addr[2] == 0x50000);
    CHECK(addr[3] == 0x50000 + 30000);
    CHECK(mod_place_samples(&info, 0x40000, 1, addr) == MOD_ERR_NO_MEMORY);

    info.sample[4].length = 0x10000;
    CHECK(mod_place_samples(&info, 0x40000, 2, addr) ==
          MOD_ERR_SAMPLE_TOO_BIG);

    /* a sample may not end on the last byte of a bank */
    memset(&info, 0, sizeof info);
    info.sample[0].length = 0xFFFF;
    info.sample[1].length = 1;
    CHECK(mod_place_samples(&info, 0x40000, 2, addr) == MOD_OK);
    CHECK(addr[0] == 0x40000);
    CHECK(addr[1] == 0x50000);
}

int main(void)
{
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
