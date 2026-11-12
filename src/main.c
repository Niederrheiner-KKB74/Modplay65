/*
 * Modplay65 - MOD and SID player for the MEGA65.
 *
 * Milestone 3: load a MOD file from the SD card, show what is in it,
 * audition single samples and play the song.
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

#ifndef MODPLAY65_VERSION
#define MODPLAY65_VERSION "dev"
#endif

/* ASCII codes delivered by the hardware key queue. */
#define KEY_RETURN 0x0D
#define KEY_DELETE 0x14
#define KEY_DOWN 0x11
#define KEY_UP 0x91
#define KEY_RIGHT 0x1D
#define KEY_LEFT 0x9D

/* Samples are auditioned at the pitch of note C-2 (Amiga period 428). */
#define AUDITION_FREQ AUDIO_DMA_FREQ(8287)

#define LIST_ROW 7
#define LIST_ROWS 16
#define COLUMN_WIDTH 40
#define ENTRY_WIDTH 37
#define STATUS_ROW 24

static struct mod_file mod;
static struct mod_player player;
static char filename[32] = "AXELF.MOD";

static uint8_t to_upper(uint8_t c)
{
    return (c >= 'a' && c <= 'z') ? (uint8_t)(c - 0x20) : c;
}

static uint8_t wait_key(void)
{
    uint8_t key;

    do {
        key = keyboard_poll();
    } while (key == 0);
    return key;
}

static void status(const char *text)
{
    screen_clear_span(0, STATUS_ROW, 79);
    screen_puts(0, STATUS_ROW, text);
}

static void all_off(void)
{
    uint8_t ch;

    for (ch = 0; ch < AUDIO_DMA_CHANNELS; ++ch) {
        audio_dma_stop(ch);
    }
}

/* Edit the file name on the given row. */
static void input_filename(uint8_t row)
{
    uint8_t len = 0;
    uint8_t key;
    uint8_t fresh = 1; /* the first typed character replaces the old name */

    while (filename[len]) {
        ++len;
    }
    screen_puts(0, row, "File on SD card:");
    for (;;) {
        screen_clear_span(17, row, sizeof(filename) + 1);
        screen_puts(17, row, filename);
        screen_reverse((uint8_t)(17 + len), row, 1, 1);
        key = wait_key();
        if (key == KEY_RETURN && len) {
            break;
        }
        if (key == KEY_DELETE && len) {
            filename[--len] = 0;
            fresh = 0;
        } else if (key > ' ' && key < 0x7F && (fresh || len < sizeof(filename) - 1)) {
            if (fresh) {
                len = 0;
                fresh = 0;
            }
            filename[len++] = (char)to_upper(key);
            filename[len] = 0;
        }
    }
    screen_reverse((uint8_t)(17 + len), row, 1, 0);
}

static void show_progress(uint32_t bytes)
{
    screen_dec(8, STATUS_ROW, bytes, 8);
}

static uint8_t entry_x(uint8_t index)
{
    return index < LIST_ROWS ? 0 : COLUMN_WIDTH;
}

static uint8_t entry_y(uint8_t index)
{
    return (uint8_t)(LIST_ROW + (index < LIST_ROWS ? index : index - LIST_ROWS));
}

static void show_module(void)
{
    const struct mod_info *info = &mod.info;
    uint8_t i;

    screen_clear();
    screen_puts(0, 0, "Modplay65 " MODPLAY65_VERSION);
    screen_puts(0, 2, "Title:");
    screen_puts(10, 2, info->name);
    screen_puts(40, 2, "File:");
    screen_puts(50, 2, filename);
    screen_puts(0, 3, "Format:");
    screen_puts(10, 3, info->signature[0] ? info->signature : "old (15)");
    screen_puts(40, 3, "Size:");
    screen_dec(50, 3, mod.size, 7);
    screen_puts(58, 3, "bytes");
    screen_puts(0, 4, "Length:");
    screen_dec(10, 4, info->song_length, 3);
    screen_puts(14, 4, "positions");
    screen_dec(25, 4, info->num_patterns, 3);
    screen_puts(29, 4, "patterns");
    screen_puts(40, 4, "Samples:");
    screen_dec(50, 4, info->sample_bytes, 7);
    screen_puts(58, 4, "bytes");

    screen_puts(0, 6, "No Name                   Length Vol L");
    screen_puts(COLUMN_WIDTH, 6, "No Name                   Length Vol L");
    for (i = 0; i < info->num_samples; ++i) {
        const struct mod_sample *s = &info->sample[i];
        uint8_t x = entry_x(i);
        uint8_t y = entry_y(i);

        screen_dec(x, y, (uint32_t)i + 1, 2);
        screen_puts((uint8_t)(x + 3), y, s->name);
        if (s->length) {
            screen_dec((uint8_t)(x + 26), y, s->length, 6);
            screen_dec((uint8_t)(x + 33), y, s->volume, 3);
            if (s->loop_length) {
                screen_puts((uint8_t)(x + 37), y, "*");
            }
        }
    }
}

static void show_selection(uint8_t index, uint8_t on)
{
    const struct mod_sample *s = &mod.info.sample[index];

    screen_reverse(entry_x(index), entry_y(index), ENTRY_WIDTH, on);
    if (!on) {
        return;
    }
    screen_clear_span(0, STATUS_ROW - 1, 79);
    screen_puts(0, STATUS_ROW - 1, "Sample    at $       loop           finetune");
    screen_dec(7, STATUS_ROW - 1, (uint32_t)index + 1, 2);
    screen_hex(14, STATUS_ROW - 1, mod.sample_addr[index], 6);
    if (s->loop_length) {
        screen_dec(26, STATUS_ROW - 1, s->loop_start, 5);
        screen_puts(31, STATUS_ROW - 1, "+");
        screen_dec(32, STATUS_ROW - 1, s->loop_length, 5);
    } else {
        screen_puts(26, STATUS_ROW - 1, "none");
    }
    if (s->finetune < 0) {
        screen_puts(46, STATUS_ROW - 1, "-");
        screen_dec(47, STATUS_ROW - 1, (uint32_t)(-s->finetune), 1);
    } else {
        screen_dec(46, STATUS_ROW - 1, (uint32_t)s->finetune, 2);
    }
}

static void play_sample(uint8_t index)
{
    const struct mod_sample *s = &mod.info.sample[index];
    uint32_t addr = mod.sample_addr[index];
    uint8_t ch;

    all_off();
    if (s->length == 0) {
        return;
    }
    /* one channel per side */
    for (ch = 0; ch < AUDIO_DMA_CHANNELS; ch += 2) {
        audio_dma_set_volume(ch, (uint8_t)(s->volume * 2), 0);
        if (s->loop_length) {
            audio_dma_play_looped(ch, addr, addr + s->loop_start,
                                  addr + s->loop_start + s->loop_length,
                                  AUDITION_FREQ);
        } else {
            audio_dma_play(ch, addr, (uint16_t)s->length, AUDITION_FREQ, 0);
        }
    }
}

/* Pattern rows are read from the copy of the file in Attic RAM. */
void mod_read_row(uint8_t pattern, uint8_t row, uint8_t *dst)
{
    lcopy(MOD_FILE_ATTIC + mod.info.pattern_offset +
              (uint32_t)pattern * MOD_PATTERN_BYTES +
              (uint16_t)row * MOD_ROW_BYTES,
          (uint32_t)(uintptr_t)dst, MOD_ROW_BYTES);
}

#define PLAY_ROW 9

static void show_position(void)
{
    uint8_t i;

    /* The engine has already moved on to the next row: show what plays. */
    screen_dec(9, PLAY_ROW, (uint32_t)player.order + 1, 3);
    screen_dec(30, PLAY_ROW, player.pattern, 3);
    screen_dec(40, PLAY_ROW, player.row, 2);
    screen_dec(51, PLAY_ROW, player.speed, 2);
    screen_dec(60, PLAY_ROW, player.tempo, 3);
    for (i = 0; i < MOD_CHANNELS; ++i) {
        const struct mod_channel *c = &player.ch[i];
        uint8_t y = (uint8_t)(PLAY_ROW + 2 + i);
        uint8_t bar = (uint8_t)(c->out_period ? c->out_volume >> 1 : 0);

        screen_dec(18, y, c->sample, 2);
        screen_dec(29, y, c->out_period, 4);
        screen_dec(42, y, c->out_volume, 2);
        screen_clear_span(46, y, 33);
        screen_clear_span(46, y, bar);
        screen_reverse(46, y, bar, 1);
    }
}

/* Play the song from the first position until the user stops it. */
static void play_song(void)
{
    uint8_t key;
    uint8_t i;

    all_off();
    screen_clear();
    screen_puts(0, 0, "Modplay65 " MODPLAY65_VERSION);
    screen_puts(0, 2, "Title:");
    screen_puts(10, 2, mod.info.name);
    screen_puts(0, PLAY_ROW, "Position");
    screen_puts(13, PLAY_ROW, "of");
    screen_dec(16, PLAY_ROW, mod.info.song_length, 3);
    screen_puts(22, PLAY_ROW, "Pattern");
    screen_puts(36, PLAY_ROW, "Row");
    screen_puts(45, PLAY_ROW, "Speed");
    screen_puts(56, PLAY_ROW, "BPM");
    for (i = 0; i < MOD_CHANNELS; ++i) {
        uint8_t y = (uint8_t)(PLAY_ROW + 2 + i);

        screen_puts(0, y, "Channel    sample     period      volume");
        screen_dec(8, y, (uint32_t)i + 1, 1);
    }
    status("SPACE = stop   cursor left/right or -/+ = previous/next position");

    mod_player_init(&player, &mod.info, 0);
    timebase_init();
    for (;;) {
        if (timebase_poll(player.tempo)) {
            mod_player_tick(&player);
            mod_out_apply(&player, &mod);
            if (player.row_started) {
                show_position();
            }
        }
        key = keyboard_poll();
        if (key == 0) {
            continue;
        }
        if (key == ' ' || key == KEY_RETURN) {
            break;
        }
        if (key == KEY_RIGHT || key == '+' || key == KEY_LEFT || key == '-') {
            uint8_t order = player.order;

            if (key == KEY_RIGHT || key == '+') {
                order = (uint8_t)(order + 1 < mod.info.song_length ? order + 1 : 0);
            } else if (order) {
                --order;
            }
            mod_out_stop();
            mod_player_init(&player, &mod.info, order);
        }
    }
    mod_out_stop();
}

/* Returns when the user asks for another file. */
static void browse_samples(void)
{
    uint8_t count = mod.info.num_samples;
    uint8_t sel = 0;
    uint8_t key;

    for (;;) {
      show_module();
      status("P = play song   Cursor or +/- = select sample   SPACE = hear it   S = stop   N = new file");
      show_selection(sel, 1);
      for (;;) {
        uint8_t next = sel;

        key = to_upper(wait_key());
        switch (key) {
        case '+':
        case KEY_DOWN: next = (uint8_t)(sel + 1 < count ? sel + 1 : 0); break;
        case '-':
        case KEY_UP: next = (uint8_t)(sel ? sel - 1 : count - 1); break;
        case KEY_RIGHT:
        case KEY_LEFT:
            if (sel >= LIST_ROWS) {
                next = (uint8_t)(sel - LIST_ROWS);
            } else if (sel + LIST_ROWS < count) {
                next = (uint8_t)(sel + LIST_ROWS);
            }
            break;
        case ' ':
        case KEY_RETURN: play_sample(sel); break;
        case 'S': all_off(); break;
        case 'N': all_off(); return;
        default: break;
        }
        if (key == 'P') {
            play_song();
            break; /* redraw the sample list */
        }
        if (next != sel) {
            show_selection(sel, 0);
            sel = next;
            show_selection(sel, 1);
        }
      }
    }
}

int main(void)
{
    enum mod_file_error err;

    mega65_io_enable();
    audio_dma_init();
    screen_init();
    keyboard_flush();

    for (;;) {
        screen_clear();
        screen_puts(0, 0, "Modplay65 " MODPLAY65_VERSION " - MOD loader");
        screen_puts(0, 4, "Type the name of a MOD file in the root directory");
        screen_puts(0, 5, "of the SD card (8.3 name) and press RETURN.");
        for (;;) {
            input_filename(2);
            status("Loading          bytes");
            err = mod_file_load(filename, &mod, show_progress);
            if (err == MOD_FILE_OK) {
                break;
            }
            status("Error: ");
            screen_puts(7, STATUS_ROW, mod_file_error_text(err));
        }
        browse_samples();
    }
}
