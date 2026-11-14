# Modplay65

[![Build](https://github.com/Niederrheiner-KKB74/Modplay65/actions/workflows/build.yml/badge.svg)](https://github.com/Niederrheiner-KKB74/Modplay65/actions/workflows/build.yml)

A music player for the [MEGA65](https://mega65.org) that plays ProTracker
MOD files and SID files, with a graphical user interface and sound
visualisation.

It plays music straight from the SD card: 4-channel MOD files through the
audio DMA channels and PSID tunes (one or two SIDs) on the built-in SID
chips. While a song plays it shows oscilloscopes per channel or SID voice,
level meters, notes, the running pattern (MOD) and the register settings
of each SID voice.

## Installation

Download `modplay65.d81` (or `modplay65.prg`) from the
[releases](https://github.com/Niederrheiner-KKB74/Modplay65/releases) or
from the artifacts of the latest
[build](https://github.com/Niederrheiner-KKB74/Modplay65/actions/workflows/build.yml).

1. Copy `modplay65.d81` to the SD card.
2. Mount it, either in the freezer (hold RESTORE for a second, choose
   drive 8) or in BASIC with `MOUNT "MODPLAY65.D81"`.
3. Load and start it: `DLOAD "MODPLAY65"`, then `RUN`.

The PRG alone can also be started with `m65 -r modplay65.prg` over the
serial/JTAG link, or in Xemu with `xmega65 -prg modplay65.prg`.

Your music does **not** go into the D81 image: copy MOD and SID files
anywhere onto the SD card (FAT32), in the root directory or in
sub-directories. Modplay65 lists files ending in `.mod` (or starting with
`mod.`) and `.sid`, plus all directories.

Requirements: a MEGA65 or Xemu. Volume, centring, stereo width and the
voice display for SID tunes need MEGA65 core **0.97 or newer**; with an
older core SID tunes still play, and the info panel says what is missing.

## Using Modplay65

The screen has a file list (top left), the player panel (bottom left) with
title, position, level meters and settings, and a display panel on the
right.

| Key | Action |
| --- | --- |
| Cursor up/down | select a file or directory |
| RETURN | play the selected file / enter the directory (`..` goes up) |
| SPACE | pause, continue |
| Cursor left/right | MOD: previous / next song position; SID: previous / next song |
| + / - | volume |
| , / . | stereo width (MOD; SID tunes with two SIDs) |
| V | switch the display panel. MOD: oscilloscopes, pattern, samples. SID: voices of the first SID, of the second SID (2SID tunes), tune info |
| S | second page of the sample list (MOD) |
| L | SID time limit: 3, 5, 10 minutes, off, 1, 2 minutes |
| Q | leave (asks first); restarts BASIC, the program is then gone from memory |

After a song the next file in the list is played. A MOD ends when it loops
back to an earlier position. A SID tune has no end, so it plays until its
time limit (shown after the playing time, 3 minutes at start). With the
limit off a SID tune plays until you choose another file.

### Limits

- MOD: 4-channel modules with 15 or 31 instruments (`M.K.`, `M!K!`, `FLT4`,
  `4CHN`). About 182 KB of sample memory are available; for bigger
  modules the largest samples are stored at half (or a quarter of) their
  sample rate, which the status line reports. Effects `E0x` (filter),
  `E3x` (glissando), `EFx` and `8xx` are ignored.
- SID: PSID files, one or two SIDs (second SID at `$D420`, `$D440` or
  `$D460`), PAL/NTSC, VBI and CIA timing. Refused with a message: RSID
  tunes, tunes without a play address and tunes located in
  `$C000-$DFFF`. Tunes that use illegal 6502 opcodes do not work, because
  the MEGA65 CPU uses those opcodes itself.
- The SID voice display is rebuilt from the register values; filter, ring
  modulation and sync are not shown.

## Documentation

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the design,
[docs/ROADMAP.md](docs/ROADMAP.md) for the milestones and
[docs/HARDWARE-NOTES.md](docs/HARDWARE-NOTES.md) for register details.

## Building

Requirements: CMake 3.18 or newer, git, and the
[llvm-mos SDK](https://github.com/llvm-mos/llvm-mos-sdk#getting-started).
The build downloads [mega65-libc](https://github.com/MEGA65/mega65-libc) by
itself.

Install the SDK (Linux example, unpacks into `$HOME/llvm-mos`):

```sh
wget https://github.com/llvm-mos/llvm-mos-sdk/releases/latest/download/llvm-mos-linux.tar.xz
tar xf llvm-mos-linux.tar.xz -C $HOME
```

Build:

```sh
cmake -DCMAKE_PREFIX_PATH=$HOME/llvm-mos -B build
cmake --build build
```

The result is `build/modplay65.prg`. A D81 image with the program is made
with

```sh
python3 tools/mkd81/mkd81.py build/modplay65.d81 "MODPLAY65,65" build/modplay65.prg
```

Unit tests for the portable code run on the PC:

```sh
cmake -S tests -B build-tests && cmake --build build-tests
ctest --test-dir build-tests --output-on-failure
```

Every push is built by GitHub Actions; the PRG and the D81 image can be
downloaded from the run's artifacts. Pushing a tag `vX.Y.Z` also creates a
GitHub release with both files.

## Source layout

```
src/        program source (C and assembly)
src/hw/     hardware access layer
src/mod/    MOD loading and replay
src/sid/    SID file parsing and playback
src/ui/     directory listing
tools/      reference tools: MOD comparison against micromod, SID
            reference run in a 6502 emulator, D81 image maker
tests/      host-side unit tests
docs/       architecture, roadmap, hardware notes
```

## License

GPL-3.0, see [LICENSE](LICENSE).

mega65-libc and the llvm-mos SDK runtime are licensed under the Apache
License 2.0 with LLVM exceptions.
