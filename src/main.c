/*
 * Modplay65 - MOD and SID player for the MEGA65.
 *
 * Main screen: file browser, player panel and a right-hand panel with
 * scopes, pattern, samples (MOD) or tune information (SID).
 *
 * The program uses no KERNAL services: text goes straight to screen RAM
 * and keys come from the hardware key queue. Q leaves the program by
 * restarting BASIC.
 */
#include <stdint.h>

#include <mega65/memory.h>

#include "hw/audio_dma.h"
#include "hw/keyboard.h"
#include "hw/memmap.h"
#include "hw/screen.h"
#include "hw/system.h"
#include "hw/timebase.h"
#include "mod/mod_file.h"
#include "mod/mod_out.h"
#include "mod/mod_play.h"
#include "sid/sid_play.h"
#include "ui/dir.h"
#include "vis/pattern.h"
#include "vis/scope.h"
#include "vis/sidvis.h"

#ifndef MODPLAY65_VERSION
#define MODPLAY65_VERSION "dev"
#endif

/* ASCII codes delivered by the hardware key queue. */
#define KEY_RETURN 0x0D
#define KEY_DOWN 0x11
#define KEY_UP 0x91
#define KEY_RIGHT 0x1D
#define KEY_LEFT 0x9D

/* Screen layout (80 x 25). */
#define FILES_X 0
#define FILES_Y 1
#define FILES_W 40
#define FILES_H 14
#define FILES_ROWS (FILES_H - 2)
#define PLAYER_X 0
#define PLAYER_Y 15
#define PLAYER_W 40
#define PLAYER_H 9
#define SAMPLES_X 40
#define SAMPLES_Y 1
#define SAMPLES_W 40
#define SAMPLES_H 23
#define SAMPLES_ROWS (SAMPLES_H - 2)
#define STATUS_ROW 24
#define BAR_LEN 30

/* Colours. */
#define C_BACK COL_BLACK
#define C_FRAME COL_LBLUE
#define C_TEXT COL_LGREY
#define C_BRIGHT COL_WHITE
#define C_LABEL COL_CYAN
#define C_TITLE COL_YELLOW
#define C_DIR COL_LGREEN
#define C_BAR COL_LGREEN
#define C_ERROR COL_PINK

static MEMMAP_HIGH struct mod_file mod;
static struct mod_player player;
static MEMMAP_HIGH struct sid_tune tune;

static uint8_t sid_loaded;     /* a SID tune is in memory             */
static uint8_t sid_running;    /* its play routine is being called    */
static uint16_t sid_frames;    /* video frames since the song started */
static uint16_t sid_seconds;
static MEMMAP_HIGH int8_t sid_wave[SIDVIS_VOICES + 1][SCOPE_POINTS];
static MEMMAP_HIGH uint8_t sid_regs[25];

static uint8_t loaded;         /* a module is in memory               */
static uint8_t playing;        /* the song is running (not paused)    */
static uint8_t sel;            /* selected entry in the file list     */
static uint8_t top;            /* first visible entry                 */
static uint8_t current = 0xFF; /* entry that is loaded, 0xFF = none   */
static uint8_t sample_page;    /* 0 = samples 1-21, 1 = 22-31         */

/* What the right-hand panel shows. */
enum { VIEW_SCOPES, VIEW_PATTERN, VIEW_SAMPLES, VIEW_COUNT };
static uint8_t view = VIEW_SCOPES;

/* Position of the row that is sounding (the engine is one row ahead). */
static uint8_t shown_order, shown_pattern, shown_row;

static uint8_t scopes_due; /* traces still to draw in this video frame */
static uint8_t master = 64;    /* 0..64                               */
static uint8_t separation = 4; /* 0..4                                */
static uint8_t level[MOD_CHANNELS];
static uint8_t marked[MOD_CHANNELS]; /* sample shown as playing, 0 = none */

/* Pattern rows are read from the copy of the file in Attic RAM. */
void mod_read_row(uint8_t pattern, uint8_t row, uint8_t *dst)
{
    lcopy(MOD_FILE_ATTIC + mod.info.pattern_offset +
              (uint32_t)pattern * MOD_PATTERN_BYTES +
              (uint16_t)row * MOD_ROW_BYTES,
          (uint32_t)(uintptr_t)dst, MOD_ROW_BYTES);
}

static void status(const char *text, uint8_t colour)
{
    screen_putn(0, STATUS_ROW, text, 79);
    screen_colour(0, STATUS_ROW, 79, colour);
}

static void help(void)
{
    if (sid_loaded) {
        status("RETURN play  SPACE pause  CRSR file/song  +- vol  ,. stereo  "
               "V view  Q quit", C_LABEL);
        return;
    }
    status("RETURN play  SPACE pause  CRSR file/position  +- vol  ,. stereo  "
           "V view  Q quit", C_LABEL);
}

/* ---- file list ------------------------------------------------------ */

static void draw_file_row(uint8_t row)
{
    uint8_t index = (uint8_t)(top + row);
    uint8_t y = (uint8_t)(FILES_Y + 1 + row);
    uint8_t x = FILES_X + 1;
    const struct dir_entry *entry;

    if (index >= dir_count) {
        screen_putn(x, y, "", FILES_W - 2);
        return;
    }
    entry = dir_entry(index);
    screen_poke(x, y, (uint8_t)(index == current ? 0x3E : 0x20)); /* > */
    screen_putn((uint8_t)(x + 1), y, entry->name, FILES_W - 8);
    screen_putn((uint8_t)(x + FILES_W - 7), y, entry->is_dir ? "<DIR>" : "",
                5);
    screen_colour(x, y, FILES_W - 2, entry->is_dir ? C_DIR : C_TEXT);
    screen_reverse(x, y, FILES_W - 2, (uint8_t)(index == sel));
}

static void draw_files(void)
{
    uint8_t row;

    if (sel < top) {
        top = sel;
    } else if (sel >= top + FILES_ROWS) {
        top = (uint8_t)(sel - FILES_ROWS + 1);
    }
    for (row = 0; row < FILES_ROWS; ++row) {
        draw_file_row(row);
    }
}

static void read_directory(void)
{
    status("Reading directory ...", C_LABEL);
    dir_read();
    sel = 0;
    top = 0;
    current = 0xFF;
    draw_files();
    if (dir_count == 0) {
        status("No MOD files or directories found here.", C_ERROR);
    } else {
        help();
    }
}

/* ---- sample list ---------------------------------------------------- */

static void draw_sample_row(uint8_t row)
{
    uint8_t index = (uint8_t)(sample_page * SAMPLES_ROWS + row);
    uint8_t y = (uint8_t)(SAMPLES_Y + 1 + row);
    uint8_t x = SAMPLES_X + 1;
    const struct mod_sample *s;

    screen_putn(x, y, "", SAMPLES_W - 2);
    if (!loaded || index >= mod.info.num_samples) {
        return;
    }
    s = &mod.info.sample[index];
    screen_dec((uint8_t)(x + 2), y, (uint32_t)index + 1, 2);
    screen_putn((uint8_t)(x + 5), y, s->name, MOD_SAMPLE_NAME_LEN);
    if (s->length) {
        screen_dec((uint8_t)(x + 28), y, s->length, 6);
        if (s->loop_length) {
            screen_poke((uint8_t)(x + 35), y, 0x2A); /* * = looped */
        }
        if (mod.sample_shift[index]) {
            screen_puts((uint8_t)(x + 36), y,
                        mod.sample_shift[index] == 1 ? "h" : "q");
        }
    }
    screen_colour(x, y, SAMPLES_W - 2, s->length ? C_TEXT : C_FRAME);
}

static void draw_samples(void)
{
    uint8_t row;

    for (row = 0; row < SAMPLES_ROWS; ++row) {
        draw_sample_row(row);
    }
    for (row = 0; row < MOD_CHANNELS; ++row) {
        marked[row] = 0;
    }
}

/* Show next to each sample which channel is playing it. */
static void mark_samples(void)
{
    uint8_t i;
    uint8_t first = (uint8_t)(sample_page * SAMPLES_ROWS + 1);

    for (i = 0; i < MOD_CHANNELS; ++i) {
        uint8_t now = player.ch[i].sample;

        if (now == marked[i]) {
            continue;
        }
        if (marked[i] >= first && marked[i] < first + SAMPLES_ROWS) {
            draw_sample_row((uint8_t)(marked[i] - first));
        }
        marked[i] = now;
    }
    for (i = 0; i < MOD_CHANNELS; ++i) {
        uint8_t now = marked[i];

        if (now >= first && now < first + SAMPLES_ROWS) {
            uint8_t y = (uint8_t)(SAMPLES_Y + 1 + now - first);

            screen_poke(SAMPLES_X + 1, y, (uint8_t)('1' + i));
            screen_colour(SAMPLES_X + 1, y, SAMPLES_W - 2, C_BRIGHT);
        }
    }
}

/* ---- right-hand panel: scopes, pattern or samples -------------------- */

#define VX (SAMPLES_X + 1)
#define VY (SAMPLES_Y + 1)
#define SCOPE_X (SAMPLES_X + 2)

static void draw_scope_labels(void)
{
    uint8_t i;

    for (i = 0; i < MOD_CHANNELS; ++i) {
        const struct mod_channel *c = &player.ch[i];
        uint8_t y = (uint8_t)(VY + i * SCOPE_ROW_STEP + SCOPE_CELLS_H);

        screen_dec(SCOPE_X + 8, y, c->sample, 2);
        if (c->sample && loaded) {
            screen_putn(SCOPE_X + 11, y, mod.info.sample[c->sample - 1].name,
                        20);
        }
        pattern_note_name(SCOPE_X + 32, y, c->out_period);
    }
}

static void draw_pattern_view(void)
{
    if (!loaded) {
        return;
    }
    pattern_draw(VX, VY, shown_pattern, shown_row);
}

static void put_hex4(uint8_t x, uint8_t y, uint16_t value)
{
    screen_puts(x, y, "$");
    screen_hex((uint8_t)(x + 1), y, value, 4);
}

/* Timing of the current song: "VBI 50 Hz" or "CIA 60 Hz". */
static void draw_sid_timing(uint8_t x, uint8_t y)
{
    screen_puts(x, y, tune.cia ? "CIA" : "VBI");
    screen_dec((uint8_t)(x + 4), y, (uint32_t)(tune.rate16 + 8) >> 4, 3);
    screen_puts((uint8_t)(x + 8), y, "Hz");
}

static void draw_sid_info(void)
{
    static const char *const labels[] = {
        "Title", "Author", "Released", "", "Format", "Songs", "Load",
        "Init", "Play", "Timing", "Clock", "SID model"
    };
    static const char *const clocks[] = { "unknown", "PAL", "NTSC", "PAL+NTSC" };
    static const char *const models[] = { "unknown", "6581", "8580", "6581+8580" };
    const struct psid_info *info = &tune.info;
    uint8_t i;

    for (i = 0; i < sizeof(labels) / sizeof(labels[0]); ++i) {
        screen_puts(VX, (uint8_t)(VY + i), labels[i]);
        screen_colour(VX, (uint8_t)(VY + i), 10, C_LABEL);
    }
    screen_putn(VX + 10, VY, info->name, 28);
    screen_colour(VX + 10, VY, 28, C_TITLE);
    screen_putn(VX + 10, VY + 1, info->author, 28);
    screen_putn(VX + 10, VY + 2, info->released, 28);
    screen_puts(VX + 10, VY + 4, info->rsid ? "RSID v" : "PSID v");
    screen_dec(VX + 16, VY + 4, info->version, 1);
    screen_dec(VX + 10, VY + 5, info->songs, 3);
    screen_puts(VX + 14, VY + 5, "(start with");
    screen_dec(VX + 26, VY + 5, info->start_song, 3);
    screen_puts(VX + 29, VY + 5, ")");
    put_hex4(VX + 10, VY + 6, info->load);
    screen_puts(VX + 15, VY + 6, "-");
    put_hex4(VX + 16, VY + 6, (uint16_t)(info->load + info->length - 1));
    put_hex4(VX + 10, VY + 7, info->init);
    put_hex4(VX + 10, VY + 8, info->play);
    draw_sid_timing(VX + 10, VY + 9);
    screen_puts(VX + 10, VY + 10, clocks[info->clock]);
    screen_puts(VX + 10, VY + 11, models[info->model]);
    if (!sid_extras) {
        screen_puts(VX, VY + 14, "SID scopes, volume and stereo");
        screen_puts(VX, VY + 15, "need MEGA65 core 0.97 or newer.");
        screen_colour(VX, VY + 14, 38, C_TITLE);
        screen_colour(VX, VY + 15, 38, C_TITLE);
    } else {
        screen_puts(VX, VY + 14, "V = scopes");
        screen_colour(VX, VY + 14, 38, C_FRAME);
    }
}

#define SID_LABEL_Y(v) ((uint8_t)(VY + (v) * SCOPE_ROW_STEP + SCOPE_CELLS_H))

/* The line under each SID scope trace: what the registers say. */
static void draw_sid_labels(void)
{
    static const char wave_letters[] = "TSPN";
    const uint8_t *r = sidvis_regs;
    char text[4];
    uint8_t v, b, y;

    for (v = 0; v < SIDVIS_VOICES; ++v, r += 7) {
        y = SID_LABEL_Y(v);
        for (b = 0; b < 4; ++b) {
            text[0] = (r[4] & (0x10 << b)) ? wave_letters[b] : '.';
            text[1] = 0;
            screen_puts((uint8_t)(SCOPE_X + 4 + b), y, text);
        }
        screen_puts(SCOPE_X + 9, y, (r[4] & 1) ? "G" : ".");
        sidvis_note((uint16_t)(r[0] | (r[1] << 8)), text);
        text[3] = 0;
        screen_puts(SCOPE_X + 11, y, text);
        screen_hex(SCOPE_X + 19, y, (uint16_t)(r[2] | ((r[3] & 15) << 8)), 3);
        screen_hex(SCOPE_X + 27, y, r[5], 2);
        screen_hex(SCOPE_X + 33, y, r[6], 2);
    }
    y = SID_LABEL_Y(3);
    screen_dec(SCOPE_X + 8, y, sidvis_regs[24] & 15, 2);
    for (b = 0; b < 3; ++b) {
        text[0] = (sidvis_regs[24] & (0x10 << b)) ? "LBH"[b] : '.';
        text[1] = 0;
        screen_puts((uint8_t)(SCOPE_X + 15 + b), y, text);
    }
    screen_hex(SCOPE_X + 23, y,
               (uint16_t)((sidvis_regs[21] & 7) | (sidvis_regs[22] << 3)), 3);
    screen_hex(SCOPE_X + 31, y, sidvis_regs[23] >> 4, 1);
}

/* Fixed parts of the SID scope view. */
static void draw_sid_scopes(void)
{
    static const uint8_t colours[4] = { COL_LGREEN, COL_CYAN, COL_YELLOW, COL_PINK };
    uint8_t v, y;

    scope_show(SCOPE_X, VY);
    for (v = 0; v < 4; ++v) {
        y = SID_LABEL_Y(v);
        screen_colour(SCOPE_X, y, 36, C_TEXT);
        screen_colour(SCOPE_X, y, 3, colours[v]);
        if (v < SIDVIS_VOICES) {
            screen_puts(SCOPE_X, y, "V");
            screen_dec(SCOPE_X + 1, y, (uint32_t)v + 1, 1);
            screen_puts(SCOPE_X + 16, y, "PW");
            screen_puts(SCOPE_X + 24, y, "AD");
            screen_puts(SCOPE_X + 30, y, "SR");
            screen_colour(SCOPE_X + 16, y, 2, C_LABEL);
            screen_colour(SCOPE_X + 24, y, 2, C_LABEL);
            screen_colour(SCOPE_X + 30, y, 2, C_LABEL);
        } else {
            screen_puts(SCOPE_X, y, "Mix");
            screen_puts(SCOPE_X + 4, y, "Vol");
            screen_puts(SCOPE_X + 11, y, "Flt");
            screen_puts(SCOPE_X + 19, y, "Cut");
            screen_puts(SCOPE_X + 27, y, "Res");
            screen_colour(SCOPE_X + 4, y, 3, C_LABEL);
            screen_colour(SCOPE_X + 11, y, 3, C_LABEL);
            screen_colour(SCOPE_X + 19, y, 3, C_LABEL);
            screen_colour(SCOPE_X + 27, y, 3, C_LABEL);
        }
    }
    draw_sid_labels();
}

/* One trace of the SID scope view per call: voices 0-2, then the mix. */
static void draw_sid_trace(uint8_t trace)
{
    if (trace < SIDVIS_VOICES) {
        sidvis_wave(trace, sid_wave[trace], SCOPE_POINTS);
        scope_draw_samples(trace, sid_wave[trace],
                           (uint8_t)(sidvis_level(trace) >> 3));
    } else {
        int8_t *const waves[SIDVIS_VOICES] = { sid_wave[0], sid_wave[1],
                                               sid_wave[2] };

        sidvis_mix(sid_wave[3], SCOPE_POINTS, waves);
        scope_draw_samples(3, sid_wave[3], 32);
    }
}

/* Redraw the whole right-hand panel for the current view. */
static void draw_view(void)
{
    static const char *const titles[VIEW_COUNT] = {
        "Scopes", "Pattern", "Samples (S = more)"
    };
    uint8_t i;

    screen_box(SAMPLES_X, SAMPLES_Y, SAMPLES_W, SAMPLES_H, C_FRAME,
               !sid_loaded ? titles[view]
               : (view == VIEW_SCOPES && sid_extras) ? "SID voices" : "SID tune");
    for (i = 0; i < SAMPLES_ROWS; ++i) {
        screen_putn(VX, (uint8_t)(VY + i), "", SAMPLES_W - 2);
        screen_colour(VX, (uint8_t)(VY + i), SAMPLES_W - 2, C_TEXT);
    }
    scopes_due = 0;
    if (sid_loaded) {
        if (view == VIEW_SCOPES && sid_extras) {
            draw_sid_scopes();
        } else {
            draw_sid_info();
        }
        return;
    }
    switch (view) {
    case VIEW_SCOPES:
        scope_show(SCOPE_X, VY);
        for (i = 0; i < MOD_CHANNELS; ++i) {
            uint8_t y = (uint8_t)(VY + i * SCOPE_ROW_STEP + SCOPE_CELLS_H);

            screen_puts(SCOPE_X, y, "Ch");
            screen_dec(SCOPE_X + 3, y, (uint32_t)i + 1, 1);
            screen_colour(SCOPE_X, y, 8, C_LABEL);
        }
        if (loaded) {
            draw_scope_labels();
        }
        break;
    case VIEW_PATTERN:
        draw_pattern_view();
        break;
    default:
        draw_samples();
        break;
    }
}

/* Called when a new row starts sounding. */
static void update_view(void)
{
    switch (view) {
    case VIEW_SCOPES: draw_scope_labels(); break;
    case VIEW_PATTERN: draw_pattern_view(); break;
    default: mark_samples(); break;
    }
}

/* ---- player panel --------------------------------------------------- */

#define PX (PLAYER_X + 1)
#define PY (PLAYER_Y + 1)

static void draw_settings(void)
{
    if (sid_loaded && !sid_extras) {
        screen_putn(PX, PY + 6, "", 28);
    } else {
        screen_puts(PX, PY + 6, "Volume");
        screen_dec(PX + 7, PY + 6, ((uint16_t)master * 100) >> 6, 3);
        screen_puts(PX + 10, PY + 6, "%");
        screen_puts(PX + 14, PY + 6, "Stereo");
        if (sid_loaded && !tune.info.sid2) {
            screen_putn(PX + 21, PY + 6, "mono", 4);
        } else {
            screen_dec(PX + 21, PY + 6, (uint16_t)separation * 25, 3);
            screen_puts(PX + 24, PY + 6, "%");
        }
    }
    screen_colour(PX, PY + 6, 27, C_LABEL);
    if (sid_loaded) {
        screen_putn(PX + 28, PY + 6, sid_running ? "PLAYING" : "PAUSED", 8);
        screen_colour(PX + 28, PY + 6, 8, sid_running ? C_BAR : C_TITLE);
        return;
    }
    screen_putn(PX + 28, PY + 6,
                !loaded ? "" : playing ? "PLAYING" : "PAUSED", 8);
    screen_colour(PX, PY + 6, 27, C_LABEL);
    screen_colour(PX + 28, PY + 6, 8, playing ? C_BAR : C_TITLE);
}

static void draw_player_frame(void)
{
    uint8_t i;

    for (i = 0; i < PLAYER_H - 2; ++i) {
        screen_putn(PX, (uint8_t)(PY + i), "", PLAYER_W - 2);
        screen_colour(PX, (uint8_t)(PY + i), PLAYER_W - 2, C_TEXT);
    }
    if (sid_loaded) {
        screen_putn(PX, PY, tune.info.name, PLAYER_W - 2);
        screen_colour(PX, PY, PLAYER_W - 2, C_TITLE);
        screen_putn(PX, PY + 1, tune.info.author, PLAYER_W - 2);
        screen_putn(PX, PY + 2, tune.info.released, PLAYER_W - 2);
        screen_puts(PX, PY + 4, "Song");
        screen_puts(PX + 8, PY + 4, "/");
        screen_puts(PX + 15, PY + 4, "Time");
        screen_puts(PX + 22, PY + 4, ":");
        screen_colour(PX, PY + 4, PLAYER_W - 2, C_LABEL);
        screen_dec(PX + 9, PY + 4, tune.info.songs, 3);
    } else if (loaded) {
        screen_putn(PX, PY, mod.info.name[0] ? mod.info.name : "(no title)",
                    MOD_NAME_LEN);
        screen_colour(PX, PY, MOD_NAME_LEN, C_TITLE);
        screen_puts(PX + 22, PY,
                    mod.info.signature[0] ? mod.info.signature : "15 ins");
        screen_dec(PX + 28, PY, mod.size >> 10, 4);
        screen_puts(PX + 33, PY, "KB");
        screen_puts(PX, PY + 1, "Pos");
        screen_puts(PX + 7, PY + 1, "/");
        screen_puts(PX + 13, PY + 1, "Pat");
        screen_puts(PX + 22, PY + 1, "Row");
        screen_puts(PX + 32, PY + 1, "/");
        screen_colour(PX, PY + 1, PLAYER_W - 2, C_LABEL);
        screen_dec(PX + 8, PY + 1, mod.info.song_length, 3);
        for (i = 0; i < MOD_CHANNELS; ++i) {
            screen_dec(PX, (uint8_t)(PY + 2 + i), (uint32_t)i + 1, 1);
            screen_colour(PX, (uint8_t)(PY + 2 + i), 7, C_LABEL);
            screen_colour(PX + 7, (uint8_t)(PY + 2 + i), BAR_LEN, C_BAR);
        }
    } else {
        screen_puts(PX, PY + 2, "Select a MOD or SID file and press");
        screen_puts(PX, PY + 3, "RETURN.");
    }
    draw_settings();
}

static void draw_sid_position(void)
{
    screen_dec(PX + 5, PY + 4, tune.song, 3);
    screen_dec(PX + 20, PY + 4, sid_seconds / 60, 2);
    screen_dec(PX + 23, PY + 4, sid_seconds % 60, 2);
    if (sid_seconds % 60 < 10) {
        screen_puts(PX + 23, PY + 4, "0");
    }
    screen_colour(PX + 5, PY + 4, 3, C_BRIGHT);
    screen_colour(PX + 20, PY + 4, 5, C_BRIGHT);
}

static void draw_position(void)
{
    screen_dec(PX + 4, PY + 1, (uint32_t)shown_order + 1, 3);
    screen_dec(PX + 17, PY + 1, shown_pattern, 3);
    screen_dec(PX + 26, PY + 1, shown_row, 2);
    screen_dec(PX + 30, PY + 1, player.speed, 2);
    screen_dec(PX + 33, PY + 1, player.tempo, 3);
}

/* Level meters: jump up on a note, fall back between notes. */
static void draw_levels(void)
{
    uint8_t i;

    for (i = 0; i < MOD_CHANNELS; ++i) {
        const struct mod_channel *c = &player.ch[i];
        uint8_t y = (uint8_t)(PY + 2 + i);

        if (c->out_flags & MOD_OUT_TRIGGER) {
            level[i] = c->out_volume;
            screen_dec(PX + 2, y, c->sample, 2);
        } else if (level[i] > 3) {
            level[i] = (uint8_t)(level[i] - 3);
        } else {
            level[i] = 0;
        }
        if (level[i] > c->out_volume) {
            level[i] = c->out_volume;
        }
        screen_bar(PX + 7, y, BAR_LEN, (uint8_t)((level[i] * 15U) >> 5));
    }
}

/* ---- playback control ----------------------------------------------- */

static void stop(void)
{
    mod_out_stop();
    playing = 0;
}

static void restart_at(uint8_t order)
{
    uint8_t i;

    mod_out_stop();
    mod_player_init(&player, &mod.info, order);
    for (i = 0; i < MOD_CHANNELS; ++i) {
        level[i] = 0;
    }
    timebase_init();
    playing = 1;
    draw_settings();
}

static void stop_sid(void)
{
    if (sid_loaded) {
        sid_stop();
        sid_loaded = 0;
        sid_running = 0;
    }
}

static void start_sid_song(uint8_t song)
{
    sid_start_song(&tune, song);
    sid_set_output(master, separation);
    sidvis_reset();
    sid_frames = 0;
    sid_seconds = 0;
    sid_running = 1;
    timebase_init();
    draw_sid_position();
    if (view != VIEW_SCOPES || !sid_extras) {
        draw_sid_timing(VX + 10, VY + 9);
    }
    draw_settings();
}

static void show_progress(uint32_t bytes)
{
    screen_dec(8, STATUS_ROW, bytes >> 10, 5);
}

/* Load and start the SID file at index. Returns 1 on success. */
static uint8_t load_sid(uint8_t index)
{
    enum sid_error err;
    uint8_t old = current;

    err = sid_load(dir_entry(index)->name, &tune, show_progress);
    if (err != SID_OK) {
        draw_player_frame();
        draw_view();
        if (old != 0xFF) {
            draw_files();
        }
        status("Error: ", C_ERROR);
        screen_puts(7, STATUS_ROW, sid_error_text(err));
        return 0;
    }
    sid_loaded = 1;
    current = index;
    draw_files();
    draw_player_frame();
    draw_view();
    help();
    start_sid_song(tune.info.start_song);
    return 1;
}

/* Load and start the file at index. Returns 1 on success. */
static uint8_t load_and_play(uint8_t index)
{
    enum mod_file_error err;
    uint8_t old = current;

    stop();
    stop_sid();
    loaded = 0;
    current = 0xFF;
    status("Loading       KB", C_LABEL);
    screen_putn(18, STATUS_ROW, dir_entry(index)->name, 60);
    if (dir_is_sid(dir_entry(index)->name)) {
        return load_sid(index);
    }
    err = mod_file_load(dir_entry(index)->name, &mod, show_progress);
    if (err != MOD_FILE_OK) {
        draw_player_frame();
        draw_view();
        if (old != 0xFF) {
            draw_files();
        }
        status("Error: ", C_ERROR);
        screen_puts(7, STATUS_ROW, mod_file_error_text(err));
        return 0;
    }
    loaded = 1;
    current = index;
    sample_page = 0;
    draw_files();
    draw_player_frame();
    draw_view();
    if (mod.reduced) {
        status("   large sample(s) stored at reduced rate to fit memory "
               "(h = half, q = quarter)", C_TITLE);
        screen_dec(0, STATUS_ROW, mod.reduced, 2);
    } else {
        help();
    }
    restart_at(0);
    return 1;
}

/* Song finished: go on with the next file in the list, if there is one. */
static void play_next_file(void)
{
    uint8_t i;
    uint8_t index = current;

    for (i = 0; i < dir_count; ++i) {
        index = (uint8_t)(index + 1 < dir_count ? index + 1 : 0);
        if (!dir_entry(index)->is_dir) {
            if (index == current) {
                restart_at(0); /* the only file: play it again */
                return;
            }
            if (load_and_play(index)) {
                return;
            }
        }
    }
    stop();
    draw_settings();
}

static void activate_selection(void)
{
    if (sel >= dir_count) {
        return;
    }
    if (dir_entry(sel)->is_dir) {
        stop();
        stop_sid();
        loaded = 0;
        if (!dir_enter(dir_entry(sel)->name)) {
            status("Error: cannot open directory", C_ERROR);
        }
        draw_player_frame();
        draw_view();
        read_directory();
    } else {
        load_and_play(sel);
    }
}

static void move_selection(int8_t delta)
{
    uint8_t old_top = top;
    uint8_t old = sel;

    if (dir_count == 0) {
        return;
    }
    if (delta < 0) {
        sel = (uint8_t)(sel ? sel - 1 : dir_count - 1);
    } else {
        sel = (uint8_t)(sel + 1 < dir_count ? sel + 1 : 0);
    }
    if (sel < top || sel >= top + FILES_ROWS) {
        draw_files();
    }
    if (top == old_top) {
        draw_file_row((uint8_t)(old - top));
        draw_file_row((uint8_t)(sel - top));
    }
}

/* Ask before leaving; the music keeps its state if the answer is no. */
static void confirm_quit(void)
{
    uint8_t key;

    if (playing) {
        mod_out_mute();
    }
    if (sid_running) {
        sid_pause();
    }
    status("Leave Modplay65 and restart BASIC?  Y = yes, any other key = no",
           C_TITLE);
    do {
        key = keyboard_poll();
    } while (key == 0);
    if (key == 'y' || key == 'Y') {
        mod_out_stop();
        stop_sid();
        system_exit_to_basic();
    }
    help();
    if (playing) {
        mod_out_refresh(&player);
    }
    timebase_init();
}

static void handle_key(uint8_t key)
{
    switch (key) {
    case 'q':
    case 'Q':
        confirm_quit();
        break;
    case KEY_DOWN: move_selection(1); break;
    case KEY_UP: move_selection(-1); break;
    case KEY_RETURN: activate_selection(); break;
    case ' ':
        if (sid_loaded) {
            sid_running = !sid_running;
            if (!sid_running) {
                sid_pause();
            }
            timebase_init();
            draw_settings();
            break;
        }
        if (!loaded) {
            break;
        }
        playing = !playing;
        if (playing) {
            mod_out_refresh(&player);
            timebase_init();
        } else {
            mod_out_mute();
        }
        draw_settings();
        break;
    case KEY_RIGHT:
        if (sid_loaded) {
            start_sid_song((uint8_t)(tune.song < tune.info.songs ? tune.song + 1 : 1));
        } else if (loaded) {
            restart_at((uint8_t)(player.order + 1 < mod.info.song_length
                                     ? player.order + 1 : 0));
        }
        break;
    case KEY_LEFT:
        if (sid_loaded) {
            start_sid_song((uint8_t)(tune.song > 1 ? tune.song - 1 : tune.info.songs));
        } else if (loaded) {
            restart_at((uint8_t)(player.order ? player.order - 1 : 0));
        }
        break;
    case '+':
    case '-':
        if (key == '+') {
            master = (uint8_t)(master <= 56 ? master + 8 : 64);
        } else {
            master = (uint8_t)(master >= 8 ? master - 8 : 0);
        }
        mod_out_set_master(master);
        if (playing) {
            mod_out_refresh(&player);
        }
        sid_set_output(master, separation);
        draw_settings();
        break;
    case ',':
    case '.':
        if (key == '.' && separation < 4) {
            ++separation;
        } else if (key == ',' && separation > 0) {
            --separation;
        }
        mod_out_set_separation(separation);
        if (playing) {
            mod_out_refresh(&player);
        }
        sid_set_output(master, separation);
        draw_settings();
        break;
    case 's':
    case 'S':
        if (view == VIEW_SAMPLES && loaded &&
            mod.info.num_samples > SAMPLES_ROWS) {
            sample_page ^= 1;
            draw_samples();
        }
        break;
    case 'v':
    case 'V':
        if (sid_loaded) {
            view = (uint8_t)(view == VIEW_SCOPES ? VIEW_SAMPLES : VIEW_SCOPES);
        } else {
            view = (uint8_t)(view + 1 < VIEW_COUNT ? view + 1 : 0);
        }
        draw_view();
        break;
    default:
        break;
    }
}

int main(void)
{
    uint8_t key;

    memmap_init();
    mega65_io_enable();
    audio_dma_init();
    screen_init();
    screen_palette(C_BACK, C_BACK);
    sid_system_init();
    keyboard_flush();

    screen_putn(0, 0, " Modplay65 " MODPLAY65_VERSION, 80);
    screen_puts(46, 0, "MOD and SID player for the MEGA65");
    screen_colour(0, 0, 80, C_TITLE);
    screen_reverse(0, 0, 80, 1);
    screen_box(FILES_X, FILES_Y, FILES_W, FILES_H, C_FRAME, "Files");
    screen_box(PLAYER_X, PLAYER_Y, PLAYER_W, PLAYER_H, C_FRAME, "Player");
    draw_player_frame();
    draw_view();
    read_directory();

    for (;;) {
        if (sid_running) {
            if (timebase_poll_rate(tune.rate16)) {
                sid_play_tick();
            }
            if (timebase_new_frame()) {
                if (++sid_frames == timebase_frame_rate()) {
                    sid_frames = 0;
                    ++sid_seconds;
                    draw_sid_position();
                }
                if (sid_extras && view == VIEW_SCOPES) {
                    sid_read_regs(0, sid_regs);
                    sidvis_frame(sid_regs);
                    draw_sid_labels();
                    scopes_due = SIDVIS_VOICES + 1;
                }
            }
            /* one trace per pass, as for MOD */
            if (scopes_due) {
                draw_sid_trace((uint8_t)(SIDVIS_VOICES + 1 - scopes_due));
                --scopes_due;
            }
        }
        if (playing && timebase_poll(player.tempo)) {
            if (player.tick == 0) { /* this tick starts a new row */
                shown_order = player.order;
                shown_pattern = player.pattern;
                shown_row = player.row;
            }
            mod_player_tick(&player);
            mod_out_apply(&player, &mod);
            draw_levels();
            if (player.row_started) {
                draw_position();
                update_view();
            }
            if (player.song_looped) {
                play_next_file();
            }
        }
        /* Oscilloscopes: all four traces once per video frame, but only
         * one per pass through this loop so a tick is never late by more
         * than one trace. */
        if (playing && view == VIEW_SCOPES) {
            if (timebase_new_frame()) {
                scopes_due = MOD_CHANNELS;
            }
            if (scopes_due) {
                uint8_t ch = (uint8_t)(MOD_CHANNELS - scopes_due);

                scope_draw(ch, mod_out_dma_channel(ch),
                           player.ch[ch].out_volume);
                --scopes_due;
            }
        }
        key = keyboard_poll();
        if (key) {
            handle_key(key);
        }
    }
}
