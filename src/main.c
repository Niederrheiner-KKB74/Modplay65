/*
 * Modplay65 - MOD and SID player for the MEGA65.
 *
 * Main screen: file browser, player panel and sample list.
 *
 * The program uses no KERNAL services: text goes straight to screen RAM
 * and keys come from the hardware key queue. It does not return to BASIC;
 * reset the machine to leave.
 */
#include <stdint.h>

#include <mega65/memory.h>

#include "hw/audio_dma.h"
#include "hw/keyboard.h"
#include "hw/screen.h"
#include "hw/timebase.h"
#include "mod/mod_file.h"
#include "mod/mod_out.h"
#include "mod/mod_play.h"
#include "ui/dir.h"

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

static struct mod_file mod;
static struct mod_player player;

static uint8_t loaded;         /* a module is in memory               */
static uint8_t playing;        /* the song is running (not paused)    */
static uint8_t sel;            /* selected entry in the file list     */
static uint8_t top;            /* first visible entry                 */
static uint8_t current = 0xFF; /* entry that is loaded, 0xFF = none   */
static uint8_t sample_page;    /* 0 = samples 1-21, 1 = 22-31         */
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
    status("RETURN play  SPACE pause  CRSR file/position  +- volume  ,. stereo  "
           "S samples", C_LABEL);
}

/* ---- file list ------------------------------------------------------ */

static void draw_file_row(uint8_t row)
{
    uint8_t index = (uint8_t)(top + row);
    uint8_t y = (uint8_t)(FILES_Y + 1 + row);
    uint8_t x = FILES_X + 1;

    if (index >= dir_count) {
        screen_putn(x, y, "", FILES_W - 2);
        return;
    }
    screen_poke(x, y, (uint8_t)(index == current ? 0x3E : 0x20)); /* > */
    screen_putn((uint8_t)(x + 1), y, dir_entries[index].name, FILES_W - 8);
    screen_putn((uint8_t)(x + FILES_W - 7), y,
                dir_entries[index].is_dir ? "<DIR>" : "", 5);
    screen_colour(x, y, FILES_W - 2,
                  dir_entries[index].is_dir ? C_DIR : C_TEXT);
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

/* ---- player panel --------------------------------------------------- */

#define PX (PLAYER_X + 1)
#define PY (PLAYER_Y + 1)

static void draw_settings(void)
{
    screen_puts(PX, PY + 6, "Volume");
    screen_dec(PX + 7, PY + 6, ((uint16_t)master * 100) >> 6, 3);
    screen_puts(PX + 10, PY + 6, "%");
    screen_puts(PX + 14, PY + 6, "Stereo");
    screen_dec(PX + 21, PY + 6, (uint16_t)separation * 25, 3);
    screen_puts(PX + 24, PY + 6, "%");
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
    if (loaded) {
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
        screen_puts(PX, PY + 2, "Select a MOD file and press RETURN.");
    }
    draw_settings();
}

static void draw_position(void)
{
    screen_dec(PX + 4, PY + 1, (uint32_t)player.order + 1, 3);
    screen_dec(PX + 17, PY + 1, player.pattern, 3);
    screen_dec(PX + 26, PY + 1, player.row, 2);
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

static void show_progress(uint32_t bytes)
{
    screen_dec(8, STATUS_ROW, bytes >> 10, 5);
}

/* Load and start the file at index. Returns 1 on success. */
static uint8_t load_and_play(uint8_t index)
{
    enum mod_file_error err;
    uint8_t old = current;

    stop();
    loaded = 0;
    current = 0xFF;
    status("Loading       KB", C_LABEL);
    screen_putn(18, STATUS_ROW, dir_entries[index].name, 60);
    err = mod_file_load(dir_entries[index].name, &mod, show_progress);
    if (err != MOD_FILE_OK) {
        draw_player_frame();
        draw_samples();
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
    draw_samples();
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
        if (!dir_entries[index].is_dir) {
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
    if (dir_entries[sel].is_dir) {
        stop();
        loaded = 0;
        if (!dir_enter(dir_entries[sel].name)) {
            status("Error: cannot open directory", C_ERROR);
        }
        draw_player_frame();
        draw_samples();
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

static void handle_key(uint8_t key)
{
    switch (key) {
    case KEY_DOWN: move_selection(1); break;
    case KEY_UP: move_selection(-1); break;
    case KEY_RETURN: activate_selection(); break;
    case ' ':
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
        if (loaded) {
            restart_at((uint8_t)(player.order + 1 < mod.info.song_length
                                     ? player.order + 1 : 0));
        }
        break;
    case KEY_LEFT:
        if (loaded) {
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
        draw_settings();
        break;
    case 's':
    case 'S':
        if (loaded && mod.info.num_samples > SAMPLES_ROWS) {
            sample_page ^= 1;
            draw_samples();
        }
        break;
    default:
        break;
    }
}

int main(void)
{
    uint8_t key;

    mega65_io_enable();
    audio_dma_init();
    screen_init();
    screen_palette(C_BACK, C_BACK);
    keyboard_flush();

    screen_putn(0, 0, " Modplay65 " MODPLAY65_VERSION, 80);
    screen_puts(49, 0, "MOD player for the MEGA65");
    screen_colour(0, 0, 80, C_TITLE);
    screen_reverse(0, 0, 80, 1);
    screen_box(FILES_X, FILES_Y, FILES_W, FILES_H, C_FRAME, "Files");
    screen_box(PLAYER_X, PLAYER_Y, PLAYER_W, PLAYER_H, C_FRAME, "Player");
    screen_box(SAMPLES_X, SAMPLES_Y, SAMPLES_W, SAMPLES_H, C_FRAME,
               "Samples (S = more)");
    draw_player_frame();
    draw_samples();
    read_directory();

    for (;;) {
        if (playing && timebase_poll(player.tempo)) {
            mod_player_tick(&player);
            mod_out_apply(&player, &mod);
            draw_levels();
            if (player.row_started) {
                draw_position();
                mark_samples();
            }
            if (player.song_looped) {
                play_next_file();
            }
        }
        key = keyboard_poll();
        if (key) {
            handle_key(key);
        }
    }
}
