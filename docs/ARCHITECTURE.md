# Modplay65 architecture

Status: draft 1 (November 2026). Nothing in here has run on hardware yet.
Items marked **[verify]** are assumptions that an early milestone must
confirm on Xemu and on a real MEGA65. See [ROADMAP.md](ROADMAP.md) for the
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

The llvm-mos MEGA65 target loads the program at `$2001`, switches the BASIC
ROM out and gives the program `$2001-$CFFF` in bank 0 (about 44 KB) with the
soft stack growing down from `$D000`.

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
    sid.[ch]          SID chip access, model and mixer setup
    irq.s             interrupt entry/exit, CIA timer setup
    bigmem.[ch]       far memory helpers on top of mega65-libc
  mod/              MOD support (portable)
    mod_load.[ch]     header and sample table parsing
    mod_play.[ch]     replay engine: rows, ticks, effects
  sid/              SID support
    psid.[ch]         PSID header parsing (portable)
    sid_host.[ch]     sandbox bank setup, song selection
    sid_tramp.s       trampoline that calls init/play in the sandbox
  ui/               screen layout, file browser, input handling
  vis/              visualisers
tests/              host-side unit tests for the portable code
tools/              helper scripts (disk image creation, test data)
docs/               this documentation
```

## 5. Memory plan

| Region | Use |
| --- | --- |
| `$0002000-$000CFFF` (bank 0) | Program code, data, stack |
| `$0010000-$0011FFF` (bank 1) | DOS, do not touch |
| rest of bank 1 | Free in principle; colour RAM mirror at `$1F800` **[verify]** |
| `$0020000-$003FFFF` (banks 2-3) | ROM, write-protected |
| `$0040000-$005FFFF` (banks 4-5) | 128 KB free chip RAM: sample playback memory, SID sandbox |
| `$8000000-$87FFFFF` | 8 MB Attic RAM: whole-file storage, pattern data |

The constraint that shapes everything: audio DMA can only play samples that
sit in chip RAM (the first 384 KB), and each channel's playback window is
addressed with a 16-bit end address, so one sample cannot exceed 64 KB or
cross a bank boundary **[verify]**. Of the 384 KB, only banks 4 and 5 are
free without giving up the ROM.

Consequences for MOD playback:

- Pattern data (1 KB per pattern, often more than 60 KB in total) goes to
  Attic RAM. The current pattern is copied into a 1 KB buffer in bank 0 when
  the song position changes.
- Sample data is copied into banks 4-5. Modules with up to about 128 KB of
  samples play directly.
- Larger modules need a strategy. Options, to be decided after milestone 1:
  (a) refuse with a clear message; (b) also use the free part of bank 1;
  (c) give up the ROM, take banks 2-3 as well (384 KB total, but then
  keyboard and file access must not depend on the KERNAL);
  (d) keep all samples in Attic RAM and swap the ones needed by upcoming
  patterns into chip RAM.

## 6. MOD playback

### 6.1 Loading

- Supported at first: 31-instrument, 4-channel modules (`M.K.`, `M!K!`,
  `FLT4`, `4CHN`) and old 15-instrument Soundtracker files (no signature).
- 6- and 8-channel variants are out of scope; the hardware has four channels.
- The loader reads the file in 512-byte blocks, parses the 1084-byte header
  in bank 0, then streams patterns to Attic RAM and samples to chip RAM.

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

The replayer keeps a lookup table for the standard periods (with finetune)
and falls back to the hardware divider for slides and vibrato.

### 6.4 Timing

One tick lasts `2.5 / BPM` seconds (20 ms at the default 125 BPM). A CIA
timer interrupt drives the tick so that tempo changes (effect `Fxx` ≥ `$20`)
only reload the timer **[verify CIA clock rate on PAL and NTSC]**.

Per tick, the interrupt handler:

1. on tick 0, reads the next row from the pattern buffer and triggers notes;
2. on other ticks, applies running effects (slides, vibrato, tremolo,
   arpeggio, retrigger, note cut/delay);
3. writes frequency and volume to the DMA channel registers;
4. updates the "now playing" structure for the UI.

### 6.5 Sample details

- MOD samples are signed 8-bit, which is the DMA default.
- Loops: a looping sample first plays from its start, then repeats between
  loop start and loop end. The DMA channel has one base address, one top
  address and a loop flag, so the replayer must switch the base address to
  the loop start once the first pass is running. The exact register sequence
  is a milestone 1 experiment.
- Effect `9xx` (sample offset) sets the start address directly.

### 6.6 Effects

All standard ProTracker effects `0`-`F` including the `Exy` set. Order of
implementation is in the roadmap; correctness is checked against a reference
player on the PC using the shared portable code.

## 7. SID playback

### 7.1 The problem

A PSID file is C64 machine code with a load address, an init address and a
play address. It expects the C64 memory map and may sit anywhere in 64 KB,
very often at `$1000`. Modplay65 itself occupies `$2001-$CFFF` in bank 0, so
a tune cannot simply be loaded where it wants to be.

### 7.2 Proposed approach: sandbox bank

- The tune is loaded into bank 4 at its original 16-bit address, so bank 4
  acts as "the C64's RAM".
- A small assembly trampoline uses the 45GS02 `MAP` instruction to map
  bank 4 over the address space, calls the tune's init or play routine and
  restores the normal mapping afterwards.
- The trampoline must be visible in both mappings. PSID v2 headers name a
  free page range (`startPage`, `pageLength`); the trampoline is copied
  there, as PSID64 does on a real C64. Tunes without that information get a
  default location plus an overlap check.
- The I/O area stays visible, so the tune writes to the real SID registers
  at `$D400`.
- The play routine is called from an interrupt at the rate the header asks
  for: once per video frame, or CIA timer speed.

This is the riskiest part of the project and gets its own spike (milestone 6)
before any UI work depends on it. Open points: zero page and stack handling
while mapped, and whether the CPU should drop to 1 MHz during the call.

### 7.3 Scope

- **In:** PSID v1-v4, sub-tune selection, PAL/NTSC and 6581/8580 selection
  from the header (`$D63C` selects the SID model), 2SID and 3SID tunes (the
  usual second address `$D420` is a real second SID on the MEGA65).
- **Out, at least initially:** RSID tunes and anything else that needs a
  complete C64 environment (own interrupt handlers, sample playback through
  NMI, BASIC tunes). The player detects these and says so.

### 7.4 Sound routing

The MEGA65 has four SIDs: `$D400` and `$D420` on one side, `$D440` and
`$D460` on the other. SID 1 was heard on the left only in a hardware test, so a normal one-SID
tune would come out of one side only;
the audio mixer (`$D6F4/$D6F5`) is set to centre it **[verify]**.

## 8. User interface

- 80-column text screen with a custom character set; the visualiser area
  uses full-colour character mode so it can show real graphics.
- Layout: file browser, song information (title, format, sample or SID
  details), transport and position, visualiser.
- Input: keyboard first. Mouse support (mega65-libc has a mouse module) is
  an optional later step.
- Screen updates are synchronised to the video frame and never block the
  replay interrupt.

### File access

Two possible sources: files on the SD card's FAT file system (read through
hypervisor calls, 512-byte blocks, wrapped by mega65-libc `fileio`), and
files inside a mounted D81 disk image or on a real floppy (through the
KERNAL). The plan starts with the SD card, because that is where music
collections usually sit and it is the faster path. D81 support follows later.

## 9. Visualisation

| Visualiser | MOD | SID |
| --- | --- | --- |
| Level meters per voice | channel volume and note triggers | from register shadow, see below |
| Oscilloscope per voice | sample bytes read at the channel's current address (`$D72A-$D72C`) | not available per voice |
| Master oscilloscope | mixer read-back `$D6FC/$D6FD` **[verify]** | same |
| Note display | period → note per channel | frequency register → note per voice |
| Pattern view | scrolling rows | not applicable |

SID registers are write-only, so per-voice data for SID tunes needs a shadow
copy. Option: while the play routine runs, map RAM instead of I/O at
`$D400`, then copy the written values to the real chip after each call. That
gives the visualiser every register but breaks tunes that write a register
several times per call. This is decided during the SID spike; the fallback is
the master oscilloscope plus oscillator 3 and envelope 3 read-back.

## 10. Testing

- **Host unit tests** for the portable code (loaders, period tables, effect
  engine) run in CI.
- **Xemu** for interactive testing; the PRG can be started directly.
- **Real hardware** for everything audio-related, since timing and mixer
  behaviour are where the emulator is most likely to differ.
- A small set of freely redistributable test modules and SID files lives in
  `tests/data`; nothing with unclear licence is committed.

## 11. Open decisions

1. Strategy for modules with more than 128 KB of samples (section 5).
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
