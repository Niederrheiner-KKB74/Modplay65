/* psidinfo - print what the parser makes of a SID file. Development aid. */
#include <stdio.h>
#include <stdlib.h>

#include "sid/psid.h"

int main(int argc, char **argv)
{
    static uint8_t buf[0x10000 + 0x200];
    struct psid_info info;
    FILE *f;
    size_t n;
    int i, e;

    for (i = 1; i < argc; ++i) {
        if (!(f = fopen(argv[i], "rb"))) {
            perror(argv[i]);
            continue;
        }
        n = fread(buf, 1, sizeof buf, f);
        fclose(f);
        e = psid_parse(buf, n > 0x200 ? 0x200 : (uint16_t)n, (uint32_t)n, &info);
        printf("%s: error %d\n", argv[i], e);
        if (e) {
            continue;
        }
        printf("  %s v%d \"%s\" / \"%s\" / \"%s\"\n", info.rsid ? "RSID" : "PSID",
               info.version, info.name, info.author, info.released);
        printf("  load $%04X-$%04X init $%04X play $%04X songs %d start %d "
               "speed $%08lX clock %d model %d free $%02X+%d sid2 $%04X\n",
               info.load, info.load + info.length - 1, info.init, info.play,
               info.songs, info.start_song, (unsigned long)info.speed,
               info.clock, info.model, info.start_page, info.page_length,
               info.sid2);
    }
    return 0;
}
