# Modplay65 architecture

Status: draft 8 (14 November 2026). MOD playback with visualisers and PSID
playback work on real hardware; SID visualisation and SID levels are new
and wait for a hardware test. Missing: RSID tunes. Items marked **[verify]** are assumptions that an early
milestone must confirm on Xemu and on a real MEGA65. See [ROADMAP.md](ROADMAP.md) for the
order of work and [HARDWARE-NOTES.md](HARDWARE-NOTES.md) for register details.

## 1. Goal

A native MEGA65 program that

- plays ProTracker-style MOD files through the four audio DMA channels,
- plays SID files (PSID) on the built-in SID chips,
- offers a graphical user interface (file browser, transport, song info),
- visualises the sound while it plays.

## 2. Toolchain

| Part | Choice |
| --- | --- |
| Compiler | llvm-mos (`mos-mega65-clang`), SDK v23.4.0 or newer |
| Build | CMake, one target `modplay65.prg` |
| Library | mega65-libc (pinned commit, fetched by CMake) |
| Languages | C for logic and UI, 45GS02 assembly for interrupt handlers and the SID trampoline |
| CI | GitHub Actions builds the PRG on every push and pull request |

The llvm-mos MEGA65 target loads the program at `$2001` and switches the
BASIC ROM out. Modplay65 links with its own script: program and data in
`$2001-$B4FF`, soft stack growing down from `$B500`, `$B500-$CEFF` for the
screen's glyph block, `$CF00-$CFFF` for the trampoline page and a
not-loaded section `.hibss` at `$E000-$FFF9` for large buffers. Most code is
built with `-Oz`; the code that runs every tick or frame with `-Os`.

## 3. Design principles

1. **Portable core, thin hardware layer.** File format parsing and the MOD
   replay logic are plain C with no hardware access. They talk to the machine
   through a small interface (`hw/`). This lets the same code compile natively
   on a PC, so parsers and the effect engine get unit tests in CI without an
   emulator.
2. **Interrupt code is short.** The replay tick runs in an interrupt; the UI
   and visualiser run in the main loop and only read state that the interrupt
   publishes (a small "now playing" structure, written with interrupts in
   mind).
3. **Large data lives outside bank 0.** Bank 0 holds code and small working
   buffers only. Module data is kept in other banks or Attic RAM and reached
   with DMA copies and 32-bit pointers.

## 4. Source layout

Only the directories marked "exists" are in the repository today; the others
are created when their milestone starts.

```
src/
  main.c            program entry, main loop                       (exists)
  hw/               hardware layer                                  (exists)
    audio_dma.[ch]    audio DMA channel registers and helpers       (exists)
    keyboard.h        hardware key queue                            (exists)
    screen.[ch]       direct text output to screen RAM              (exists)
    timebase.[ch]     tick timing from the raster counter           (exists)
    fileload.[ch]     whole file from SD card to Attic RAM          (exists)
    memmap.[ch]       memory maps, trampoline page, IRQ stub        (exists)
    sid.[ch]          SID chip access, model and mixer setup
    irq.s             interrupt entry/exit, CIA timer setup
    bigmem.[ch]       far memory helpers on top of mega65-libc
  mod/              MOD support (portable)
    mod_load.[ch]     header parsing, sample placement              (exists)
    mod_file.[ch]     SD card loading (MEGA65 only)                 (exists)
    mod_play.[ch]     replay engine: rows, ticks, effects           (exists)
    mod_tables.[ch]   period and waveform tables                    (exists)
    mod_out.[ch]      engine output to audio DMA (MEGA65 only)      (exists)
  sid/              SID support                                   (exists)
    psid.[ch]         PSID header parsing (portable)
    sid_play.[ch]     bank 4 setup, song start, play calls, SID levels
  ui/               directory listing (dir.[ch]); the screen layout and
                    key handling are still in main.c              (exists)
  vis/              scope.[ch], pattern.[ch], sidvis.[ch]         (exists)
tests/              host-side unit tests for the portable code      (exists)
tools/              helper scripts (disk image creation, test data)
docs/               this documentation
```

## 5. Memory plan

| Region | Use |
| --- | --- |
| `$0000800-$00017FF` (bank 0) | Screen RAM (4000 bytes in 16-bit character mode) |
| `$0002001-$000B4FF` (bank 0) | Program code, data, stack (own linker script `src/modplay65.ld`); about 36 KB of 37 KB used (Nov 2026) |
| `$000B500-$000CEFF` (bank 0) | Glyph block: character set copy and pixel canvas (fixed address, see HARDWARE-NOTES.md) |
| `$000CF00-$000CFFF` (bank 0) | Trampoline page: memory-map switches, SID call, IRQ stub (visible in the tune's memory map too) |
| `$000E000-$000FFF9` (bank 0) | Large buffers (`.hibss`, not loaded): module and tune headers, directory, file buffer, scope and SID-visualiser buffers; RAM because Modplay65 switches the KERNAL off (`$01 = $35`, block 7 unmapped) and points the CPU vectors to the IRQ stub; BASIC is restored on exit |
| `$0010000-$0011FFF` (bank 1) | DOS, do not touch |
| `$0012000-$001F7FF` (bank 1) | 54 KB sample playback memory (below the colour RAM window at `$1F800`) |
| `$0020000-$003FFFF` (banks 2-3) | ROM, write-protected |
| `$0040000-$005FFFF` (banks 4-5) | 128 KB free chip RAM: MOD sample memory, or bank 4 as the SID tune's C64 memory |
| `$8000000-$87FFFFF` | 8 MB Attic RAM: whole-file storage, pattern data; directory list at `$87E0000` |

The constraint that shapes everything: audio DMA can only play samples that
sit in chip RAM (the first 384 KB), and each channel's playback window is
addressed with a 16-bit end address, so one sample cannot exceed 64 KB or
cross a bank boundary **[verify]**. Of the 384 KB, only banks 4 and 5 are
free without giving up the ROM.

Consequences for MOD playback:

- Pattern data (1 KB per pattern, often more than 60 KB in total) goes to
  Attic RAM. The current pattern is copied into a 1 KB buffer in bank 0 when
  the song position changes.
- Sample data is copied into banks 4-5 and the free part of bank 1, about
  182 KB in total. Samples are placed largest first; each stays inside one
  bank. Six of the first eight test modules need more than 128 KB, one
  (300 KB) does not fit and is refused with a message.
- Modules above 182 KB: the largest samples are stored at half their
  sample rate (averaging neighbouring bytes), largest first and leaving
  samples under 2 KB alone, until everything fits; a quarter rate is the
  last resort. Playback halves the frequency, offsets and loop points of
  such a sample. This costs treble on the reduced samples but plays any
  module; the screen marks reduced samples.

## 6. MOD playback

### 6.1 Loading

- Supported at first: 31-instrument, 4-channel modules (`M.K.`, `M!K!`,
  `FLT4`, `4CHN`) and old 15-instrument Soundtracker files (no signature).
- 6- and 8-channel variants are out of scope; the hardware has four channels.
- The loader reads the whole file in 512-byte blocks into Attic RAM at
  `$8000000`, parses the 1084-byte header in bank 0 and copies the samples
  from Attic RAM to chip RAM. Patterns are read from the Attic copy.
- Sample placement: largest first into three regions (see section 5); a
  sample never crosses a 64 KB boundary and never ends on the last byte of
  a bank.

### 6.2 Channel mapping

Amiga channels 0 and 3 are left, 1 and 2 are right. MEGA65 audio DMA
channels 0-1 play left and 2-3 play right (confirmed on hardware), each
with a cross-feed volume
register (`$D71C-$D71F`). Mapping: Amiga 0 → DMA 0, Amiga 3 → DMA 1,
Amiga 1 → DMA 2, Amiga 2 → DMA 3. The cross-feed registers give an
adjustable stereo separation, which is a user setting.

### 6.3 Pitch

An Amiga period `p` plays at `3546895 / p` Hz (PAL). The DMA frequency
register is a 24-bit increment added on every cycle of the 40.5 MHz clock
**[verify]**, so

```
FREQ = rate * 2^24 / 40500000  ≈  1469309 / p
```

The replayer keeps a period table for all 16 finetune values and computes
the frequency register with one 32-bit division per change.

### 6.4 Timing

One tick lasts `2.5 / BPM` seconds (20 ms at the default 125 BPM).

Current implementation: the main loop polls the VIC raster line counter
(`$D011/$D012`), adds `lines * BPM` to an accumulator and runs a tick each
time it passes 2.5 seconds worth of lines (PAL 15625 lines/s, NTSC 15734).
This needs no interrupts, has about 64 µs resolution and behaves the same
on hardware and in Xemu. CIA timers were rejected as timebase because
Xemu does not run them at the hardware rate. The main loop must come back
to the poll at least once per video frame.

Planned: move the tick into a raster interrupt when screen updates become
long enough to delay it.

Per tick, the player:

1. on tick 0, reads the next row from the pattern buffer and triggers notes;
2. on other ticks, applies running effects (slides, vibrato, tremolo,
   arpeggio, retrigger, note cut/delay);
3. writes frequency and volume to the DMA channel registers;
4. updates the "now playing" structure for the UI.

### 6.5 Sample details

- MOD samples are signed 8-bit, which is the DMA default.
- Loops: a looping sample first plays from its start, then repeats between
  loop start and loop end. The DMA channel has one base address, one top
  address and a loop flag, so the replayer starts a looping window that ends
  at the loop end and then rewrites the base address to the loop start
  (confirmed on hardware, see HARDWARE-NOTES.md).
- Effect `9xx` (sample offset) sets the start address directly.

### 6.6 Effects

All standard ProTracker effects `0`-`F` including the `Exy` set. Order of
implementation is in the roadmap. Correctness is checked on the PC against
the micromod reference replay, tick by tick (`tools/refcompare`).

## 7. SID playback

### 7.1 The problem

A PSID file is C64 machine code with a load address, an init address and a
play address. It expects the C64 memory map and may sit anywhere in 64 KB,
very often at `$1000`. Modplay65 itself occupies `$2001-$CFFF` in bank 0, so
a tune cannot simply be loaded where it wants to be.

### 7.2 Implementation: bank 4 as C64 memory (milestone 6)

- The tune is copied into chip RAM bank 4 (`$40000-$4FFFF`) at its own
  16-bit address; bank 4 is "the C64's RAM". This is the same memory the
  MOD player uses for samples, so only one of the two can be loaded.
- `src/hw/memmap.c` writes the trampoline page to `$CF00`. Before each
  call of init or play it executes `MAP` so that the CPU sees bank 4
  instead of bank 0:

  | Block | Address | Tune's map | Normal map |
  | --- | --- | --- | --- |
  | 0 | `$0000-$1FFF` | bank 4 | bank 0 |
  | 1-3 | `$2000-$7FFF` | bank 4 | bank 0 (mapped 1:1) |
  | 4-5 | `$8000-$BFFF` | bank 4 | bank 0 |
  | 6 | `$C000-$DFFF` | **not mapped**: bank 0 RAM and I/O | same |
  | 7 | `$E000-$FFFF` | bank 4 | bank 0 RAM (`$01 = $35`); C65 KERNAL only for BASIC |

  MAP takes priority over the I/O area, so block 6 must stay unmapped or
  the tune's SID writes would land in RAM. The trampoline lives in block 6
  and is therefore visible in both maps. The normal map is the one BASIC 65
  leaves for a program, except that block 7 is unmapped so Modplay65 can
  use the RAM at `$E000`; exit restores BASIC's map first.
- With block 0 mapped, the tune has its own zero page and stack page in
  bank 4, and `$00/$01` are plain RAM (the CPU port is only visible while
  block 0 is unmapped), so a tune cannot change the real ROM/I/O banking.
- After the call the trampoline does `SEI`, `CLD`, restores the normal map
  and clears Z, which the compiled code relies on.
- Interrupts: Modplay65 runs with interrupts disabled. Before init all chip
  interrupt sources are switched off, and the tune's vectors at
  `$FFFA-$FFFF` point to a stub that acknowledges and returns, in case a
  tune enables interrupts itself.
- Timing: VBI tunes are called at 50.125 Hz (PAL) or 59.826 Hz (NTSC),
  CIA tunes at the rate of the CIA1 timer A value their init routine set
  (timer stopped and loaded with `$FFFF` beforehand to detect it). The
  rate comes from the raster line counter (`timebase_poll_rate`), so it
  does not depend on the MEGA65's own video mode.
- The tune runs at the full 40.5 MHz. No problems with the test tunes.

Verified (November 2026): Commando (songs 1 and 2) and Katakis produce,
in Xemu, exactly the SID register states of a reference run in an
independent 6502 emulator (`tools/sidref`), at 50 play calls per second.
Confirmed by ear on real hardware on 14 November 2026.

### 7.3 Scope and limits

- **Supported:** PSID v1-v4, sub-tune selection, PAL/NTSC timing, SID model
  6581/8580 (`$D63C`), CIA and VBI timing.
- **Refused with a message:** RSID tunes; tunes with play address 0 (they
  install their own interrupt); tunes whose data, init or play lie in
  `$C000-$DFFF`; second or third SID at an address other than `$D420`,
  `$D440`, `$D460`.
- **Known gaps:** a tune that writes to `$C000-$CFFF` outside its own data
  overwrites Modplay65's glyph block (screen garbage, not a crash). Tunes
  that use illegal 6502 opcodes fail, because those opcodes are 45GS02
  instructions. 2SID/3SID tunes are untested.
- Pause switches off waveform and gate of the voices but keeps the volume
  register, because many tunes set it only once in their init routine.

### 7.4 Sound routing

The MEGA65 has four SIDs: `$D400` and `$D420` on the left, `$D440` and
`$D460` on the right. Since core 0.97 each SID has two level registers,
`$xx1D` (left) and `$xx1E` (right), reachable while bit 4 of `$D63C` is
set. Modplay65 uses them instead of the audio mixer, so the user's mixer
setting is never touched:

- one-SID tunes: both levels of SID 1 at `$C0` x volume, i.e. centred;
- 2SID tunes: SID 1 left, the second SID right; the stereo setting (`,`
  and `.`) adds 0-100 % of each to the other side;
- `+`/`-` scale all levels (volume).

The original levels and `$D63C` are saved at start and restored when a
MOD is played or the program exits. On cores before 0.97 these registers
do not exist; SID tunes then play with the hardware default routing and
the info panel says so.

## 8. User interface

- 80-column screen in the VIC-IV's 16-bit character mode, written directly
  to screen and colour RAM (two bytes per cell each). Characters 0-255
  are a RAM copy of the ROM set with lower and upper case.
- Pixel canvas: characters from 256 on have their glyphs in the same RAM
  block, so a rectangle of cells can be drawn into pixel by pixel (one
  colour per cell). 576 cells, used as 4 traces of 288 x 32 pixels.
- Layout: file list (top left), player panel with position, level meters
  and settings (bottom left), and a right-hand panel that V switches
  between oscilloscopes, pattern view and sample list.
- Input: keyboard, read from the hardware key queue. Mouse support is an
  optional later step.
- Screen updates happen in the main loop between ticks. Because the tick
  is polled (section 6.4), no pass through the loop may take longer than
  one video frame; the scopes therefore draw one trace per pass and avoid
  multiplications.

### File access

Two possible sources: files on the SD card's FAT file system (read through
hypervisor calls, 512-byte blocks, wrapped by mega65-libc `fileio`), and
files inside a mounted D81 disk image or on a real floppy (through the
KERNAL). The plan starts with the SD card, because that is where music
collections usually sit and it is the faster path. D81 support follows later.

## 9. Visualisation

| Visualiser | MOD | SID |
| --- | --- | --- |
| Level meters per voice | done: note triggers and volume | done: envelope model (below) |
| Oscilloscope per voice | done: sample bytes at the channel's playhead, scaled by volume | done: waveform rebuilt from the registers |
| Mix trace | - | done: sum of the three voices x master volume |
| Note display | done | done: frequency → note, plus waveform, pulse width, ADSR, filter |
| Pattern view | done | not applicable |

SID registers are write-only, but since core 0.97 the last written values
can be read at `$FF65000 + 32 x SID` (loopback). After every play call
the visualiser (`src/vis/sidvis.c`) copies SID 1's 25 registers and

- runs a simple ADSR model per voice, one step per video frame, using the
  SID's attack/decay/release times (linear, no exponential decay);
- rebuilds 144 points of each voice's waveform at 8000 points per second:
  triangle, sawtooth, pulse with the pulse width, noise from a small LFSR,
  combined waveforms ANDed; scaled by the envelope;
- adds the three voices, scaled by the master volume, for the mix trace.

Filter, ring modulation and sync are not modelled; the traces show what
the voices play, not the exact output. On cores before 0.97 the loopback
reads `$FF` and the visualiser is replaced by a message.

## 10. Testing

- **Host unit tests** for the portable code (loaders, period tables, effect
  engine) run in CI.
- **Xemu** for interactive testing; the PRG can be started directly.
- **Real hardware** for everything audio-related, since timing and mixer
  behaviour are where the emulator is most likely to differ.
- A small set of freely redistributable test modules and SID files lives in
  `tests/data`; nothing with unclear licence is committed.

## 11. Open decisions

1. (decided) Modules with more than 182 KB of samples: reduced sample
   rate, see section 5.
2. SD card only, or D81 support from the start (section 8).
3. Text-mode UI with graphics area as proposed, or a full bitmap UI.
4. Whether to support the MEGA65's PAL and NTSC video modes equally from the
   start (affects SID speed and CIA timer constants).

## 12. Sources

- MEGA65 register list: `iomap.txt` in <https://github.com/MEGA65/mega65-core>
- Audio DMA overview: <https://m65digest.substack.com/p/the-justified-ancients-of-mu-mu>
- llvm-mos SDK, MEGA65 platform: <https://github.com/llvm-mos/llvm-mos-sdk>
- mega65-libc: <https://github.com/MEGA65/mega65-libc>
- Project template this build is modelled on: <https://github.com/mlund/llvm-mos-template>
