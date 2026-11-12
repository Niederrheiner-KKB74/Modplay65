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
- [ ] Play one raw 8-bit sample from bank 4, one-shot and looped
      (test bench keys 1 and T; works in Xemu, hardware check pending)
- [ ] Confirm the frequency formula by measuring a known tone
- [ ] Work out the register sequence for "play once, then loop a sub-range"
      (test bench key 2; works in Xemu, hardware check pending)
- [ ] Confirm the memory limits (384 KB, 64 KB window) and which side is left
- [ ] Record the findings in HARDWARE-NOTES.md

## M2 - MOD loader

- [ ] Portable header and sample table parser with host unit tests
- [ ] SD card file reading in 512-byte blocks
- [ ] Patterns to Attic RAM, samples to chip RAM
- [ ] Clear error messages for unsupported or oversized modules
- [ ] Show module name, sample list and song length on screen

## M3 - MOD replay engine

- [ ] CIA timer tick interrupt, speed and tempo handling
- [ ] Notes, sample triggering, volume, loops
- [ ] Effects in this order: `Cxx` `Fxx` `Bxx` `Dxx`, then `1xx` `2xx` `3xx`
      `Axy` `5xy`, then `0xy` `4xy` `6xy` `7xy` `9xx`, then the `Exy` set
- [ ] Host-side reference test: same engine on the PC, compared row by row
      with a known-good player
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
