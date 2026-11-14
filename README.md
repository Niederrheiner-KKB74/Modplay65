# Modplay65

[![Build](https://github.com/Niederrheiner-KKB74/Modplay65/actions/workflows/build.yml/badge.svg)](https://github.com/Niederrheiner-KKB74/Modplay65/actions/workflows/build.yml)

A music player for the [MEGA65](https://mega65.org) that plays ProTracker
MOD files and SID files, with a graphical user interface and sound
visualisation.

SID limits for now: RSID tunes, tunes that bring their own interrupt
handler and tunes located in `$C000-$DFFF` are refused with a message.

**Status: MOD playback works.** The program lists the MOD files on the SD
card, plays 4-channel modules with all common ProTracker effects and shows
an oscilloscope per channel, the running pattern, level meters and the
sample list. PSID tunes (`.sid`) play on the MEGA65's SID chips with
sub-tune selection, centred output, volume control and per-voice
scopes. The SID extras need MEGA65 core 0.97 or newer.

## Keys

| Key | Action |
| --- | --- |
| Cursor up/down | select a file or directory |
| RETURN | play the selected file / enter the directory |
| SPACE | pause, continue |
| Cursor left/right | MOD: previous / next song position; SID: previous / next song |
| + / - | volume |
| , / . | stereo separation |
| V | switch the right-hand panel: MOD: oscilloscopes, pattern, samples; SID: voices, tune info |
| S | second page of the sample list |
| Q | leave (asks first); restarts BASIC, the program is then gone from memory |

## Planned features

- MOD playback (4 channels, 15 and 31 instruments) through the MEGA65 audio
  DMA channels
- SID playback (PSID) on the built-in SID chips, including multi-SID tunes
- File browser, transport controls, song information
- Visualisers: level meters, oscilloscopes, pattern view, note display

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

The result is `build/modplay65.prg`.

Unit tests for the portable code run on the PC:

```sh
cmake -S tests -B build-tests && cmake --build build-tests
ctest --test-dir build-tests --output-on-failure
```

Every push is also built by GitHub Actions; the PRG can be downloaded from
the run's artifacts.

## Running

In the [Xemu](https://github.com/lgblgblgb/xemu) MEGA65 emulator:

```sh
xmega65 -prg build/modplay65.prg
```

On a real MEGA65, copy the PRG to the SD card or into a D81 image, or send
it over the serial/JTAG link with the `m65` tool from mega65-tools.

## Source layout

```
src/        program source (C and assembly)
src/hw/     hardware access layer
src/mod/    MOD loading and replay
src/sid/    SID file parsing and playback
src/ui/     directory listing
tools/      reference tools: MOD comparison against micromod, SID
            reference run in a 6502 emulator
tests/      host-side unit tests
docs/       architecture, roadmap, hardware notes
```

## License

GPL-3.0, see [LICENSE](LICENSE).

mega65-libc and the llvm-mos SDK runtime are licensed under the Apache
License 2.0 with LLVM exceptions.
