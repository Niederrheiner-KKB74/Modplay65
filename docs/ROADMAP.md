# Roadmap

Each milestone ends with something that can be run and judged. Milestones 1
and 6 are spikes: their purpose is to replace the **[verify]** assumptions in
[ARCHITECTURE.md](ARCHITECTURE.md) with tested facts before more code is
built on top of them.

## M0 - Project skeleton (this commit)

- [x] Architecture plan, hardware notes, roadmap
- [x] CMake build with llvm-mos and mega65-libc
- [x] GitHub Actions build that publishes `modplay65.prg`
- [x] Starter program: system info and an audio DMA test tone

## M1 - Audio DMA spike

- [x] Starter program runs in Xemu: screen, keys, looped sample playback
      (registers and playhead checked; sound not yet confirmed by ear)
- [x] Test tone confirmed by ear on real hardware (12 Nov 2026)
- [x] Play one raw 8-bit sample from bank 4, one-shot and looped
      (test bench keys 1 and T; Xemu and real hardware, 12 Nov 2026)
- [ ] Confirm the frequency formula by measuring a known tone
- [x] Work out the register sequence for "play once, then loop a sub-range"
      (test bench key 2; Xemu and real hardware, 12 Nov 2026)
- [x] Which side is left: DMA channels 0/1 left, 2/3 right (hardware,
      12 Nov 2026)
- [ ] Confirm the memory limits (384 KB, 64 KB window)
- [ ] Record the findings in HARDWARE-NOTES.md

## M2 - MOD loader

- [x] Portable header and sample table parser with host unit tests
- [x] SD card file reading in 512-byte blocks
- [x] Whole file to Attic RAM (patterns stay there), samples to chip RAM
- [x] Clear error messages for unsupported or oversized modules
- [x] Show module name, sample list and song length on screen
- [x] Audition single samples (not planned originally, useful for testing)
- [x] Confirmed on real hardware with a 31-instrument file (12 Nov 2026)
- [ ] Test a 15-instrument file and a file with looped samples

## M3 - MOD replay engine

- [x] Tick timing, speed and tempo handling. Driven by polling the raster
      line counter in the main loop, not by a CIA interrupt (see
      ARCHITECTURE.md 6.4); 250 rows in 30 s measured in Xemu at 125 BPM
- [x] Notes, sample triggering, volume, loops
- [x] All ProTracker effects `0`-`F` and the `Exy` set, except `E0x`
      (filter), `E3x` (glissando), `EFx` and `8xx`, which are ignored
- [x] Host unit tests for the engine; `modtrace` tool to run a file
      through the engine on the PC
- [ ] Confirm on real hardware by ear
- [ ] Test with modules that use more effects (the first test file only
      uses `Fxx`) and looped samples
- [ ] Host-side reference test against a known-good player
- [ ] Move the tick into an interrupt once the UI needs longer screen
      updates
- [ ] Decide the large-module strategy (ARCHITECTURE.md, open decision 1)

## M4 - User interface

- [ ] Screen layout and custom character set
- [ ] File browser with directory navigation
- [ ] Transport: play, pause, stop, next and previous song position
- [ ] Song information panel
- [ ] Stereo separation and volume settings

## M5 - Visualisation for MOD

- [ ] Level meters per channel
- [ ] Oscilloscope per channel from the DMA playhead
- [ ] Scrolling pattern view
- [ ] Note display

## M6 - SID spike

- [ ] PSID header parser with host unit tests
- [ ] Load a tune into the sandbox bank, call init and play through the
      trampoline, hear music
- [ ] Settle zero page, stack and CPU speed handling
- [ ] Decide the register shadow question for visualisation
- [ ] Mixer setup so one-SID tunes are centred

## M7 - SID integration

- [ ] Sub-tune selection, PAL/NTSC and SID model from the header
- [ ] 2SID and 3SID tunes
- [ ] Detection and clear message for unsupported tunes (RSID and others)
- [ ] SID tunes in the file browser and the song information panel
- [ ] Visualisers for SID

## M8 - Polish and release

- [ ] Playlist and automatic advance to the next file
- [ ] D81 and floppy file access
- [ ] Optional mouse support
- [ ] D81 release image built by CI
- [ ] User documentation
