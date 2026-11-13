/* Host-side unit tests for src/sid/psid.c. */
#include <stdio.h>
#include <string.h>

#include "sid/psid.h"

static int failures;

#define CHECK(cond)                                                    \
    do {                                                               \
        if (!(cond)) {                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);     \
            ++failures;                                                \
        }                                                              \
    } while (0)

static uint8_t file[0x7C + 0x200];

static void put16(int at, unsigned v)
{
    file[at] = (uint8_t)(v >> 8);
    file[at + 1] = (uint8_t)v;
}

static void make_v2(void)
{
    memset(file, 0, sizeof file);
    memcpy(file, "PSID", 4);
    put16(4, 2);
    put16(6, 0x7C);
    put16(8, 0);       /* load address in the data */
    put16(0x0A, 0x1000);
    put16(0x0C, 0x1003);
    put16(0x0E, 3);
    put16(0x10, 2);
    put16(0x12, 0);
    put16(0x14, 0x0004); /* song 3 uses CIA */
    strcpy((char *)file + 0x16, "Test Tune");
    memcpy(file + 0x36, "Chris H\xfclsbeck", 15);
    strcpy((char *)file + 0x56, "1988 Somebody");
    put16(0x76, (1 << 2) | (2 << 4)); /* PAL, 8580 */
    file[0x78] = 0x04;
    file[0x79] = 0x10;
    file[0x7C] = 0x00; /* load address $1000 */
    file[0x7D] = 0x10;
}

static void test_v2(void)
{
    struct psid_info info;

    make_v2();
    CHECK(psid_parse(file, sizeof file, 0x7C + 0x102, &info) == PSID_OK);
    CHECK(info.rsid == 0 && info.version == 2);
    CHECK(info.load == 0x1000);
    CHECK(info.data_offset == 0x7E);
    CHECK(info.length == 0x100);
    CHECK(info.init == 0x1000 && info.play == 0x1003);
    CHECK(info.songs == 3 && info.start_song == 2);
    CHECK(!psid_song_uses_cia(&info, 1));
    CHECK(!psid_song_uses_cia(&info, 2));
    CHECK(psid_song_uses_cia(&info, 3));
    CHECK(strcmp(info.name, "Test Tune") == 0);
    CHECK(strcmp(info.author, "Chris Huelsbeck") == 0);
    CHECK(strcmp(info.released, "1988 Somebody") == 0);
    CHECK(info.clock == PSID_CLOCK_PAL && info.model == PSID_MODEL_8580);
    CHECK(info.start_page == 0x04 && info.page_length == 0x10);
    CHECK(info.sid2 == 0 && info.sid3 == 0);
}

static void test_variants(void)
{
    struct psid_info info;

    /* explicit load address, v1 header (no flags) */
    make_v2();
    put16(4, 1);
    put16(6, 0x76);
    put16(8, 0x0800);
    CHECK(psid_parse(file, sizeof file, 0x76 + 0x40, &info) == PSID_OK);
    CHECK(info.load == 0x0800 && info.data_offset == 0x76);
    CHECK(info.length == 0x40);
    CHECK(info.clock == PSID_CLOCK_UNKNOWN);

    /* RSID, v3 with a second SID at $D420, start song out of range */
    make_v2();
    memcpy(file, "RSID", 4);
    put16(4, 3);
    put16(0x10, 9);
    file[0x7A] = 0x42;
    CHECK(psid_parse(file, sizeof file, 0x7C + 0x10, &info) == PSID_OK);
    CHECK(info.rsid == 1 && info.sid2 == 0xD420);
    CHECK(info.start_song == 1);

    /* songs above 32 share the speed bit 31 */
    make_v2();
    put16(0x0E, 40);
    put16(0x12, 0x8000);
    put16(0x14, 0);
    CHECK(psid_parse(file, sizeof file, 0x7C + 0x10, &info) == PSID_OK);
    CHECK(!psid_song_uses_cia(&info, 31));
    CHECK(psid_song_uses_cia(&info, 32));
    CHECK(psid_song_uses_cia(&info, 33));
    CHECK(psid_song_uses_cia(&info, 40));

    /* errors */
    make_v2();
    CHECK(psid_parse(file, 0x40, 0x40, &info) == PSID_ERR_TOO_SHORT);
    memcpy(file, "XSID", 4);
    CHECK(psid_parse(file, sizeof file, 0x200, &info) == PSID_ERR_NOT_SID);
    make_v2();
    put16(4, 7);
    CHECK(psid_parse(file, sizeof file, 0x200, &info) == PSID_ERR_VERSION);
    make_v2();
    CHECK(psid_parse(file, sizeof file, 0x7C, &info) == PSID_ERR_NO_DATA);
    put16(8, 0xFF00);
    CHECK(psid_parse(file, sizeof file, 0x7C + 0x200, &info) ==
          PSID_ERR_TOO_LONG);
}

int main(void)
{
    test_v2();
    test_variants();
    if (failures) {
        printf("%d check(s) failed\n", failures);
        return 1;
    }
    printf("all tests passed\n");
    return 0;
}
