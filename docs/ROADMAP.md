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
- [x] Confirmed on real hardware by ear with one module (12 Nov 2026)
- [x] Sample memory extended to 182 KB (banks 4, 5 and part of bank 1)
- [x] Eight modules covering all effects except tremolo play correctly
      on real hardware, including samples in bank 1 and one module at
      reduced sample rate (13 Nov 2026)
- [ ] Tremolo (`7xy`) is untested with real music
- [x] Host-side reference test against micromod (`tools/refcompare`):
      eight modules agree on position, period (within 2 units), volume
      and note triggers on every tick
- [ ] Move the tick into an interrupt once the UI needs longer screen
      updates
- [x] Modules above 182 KB: largest samples stored at reduced rate

## M4 - User interface

- [x] Screen layout with frames and colours: file list, player panel,
      sample list (ROM character set, lower/upper case)
- [x] File browser: MOD files and sub-directories of the SD card, sorted
- [x] Transport: play, pause, previous and next song position
- [x] Song information and sample list with markers for playing samples
- [x] Volume and stereo separation settings
- [x] Simple level meters per channel
- [x] Automatic advance to the next file when a song ends
- [x] Confirmed on real hardware: file list, directory changes, long
      file names are shown (13 Nov 2026)
- [x] Sub-directories: entering, `..` and loading inside a directory work
      in Xemu with a FAT32 test image (first version showed none, because
      the library's readdir returns the wrong attribute byte)
- [ ] Long file names: shown correctly on hardware; loading a file by its
      long name not yet explicitly confirmed
- [x] Q leaves the program by restarting BASIC through the reset vector
      (confirmed on real hardware, 13 Nov 2026)
- [ ] Custom character set
- [x] Up to 255 directory entries (list kept in Attic RAM)

## M5 - Visualisation for MOD

- [x] Pixel canvas on the text screen (16-bit character mode, glyphs in
      RAM)
- [x] Oscilloscope per channel, drawn once per video frame from the data
      each audio DMA channel is about to play
- [x] Scrolling pattern view in tracker notation
- [x] Note display (under each scope trace)
- [x] V switches the right-hand panel: scopes, pattern, samples
- [x] Level meters per channel (player panel, since M4)
- [x] Confirmed on real hardware (13 Nov 2026): all three views, tempo
      unchanged with scopes running. The first hardware test showed wrong
      characters because the glyph block lay in a range the VIC-IV reads
      from ROM; fixed by moving it to `$B600`
- [ ] Tick in an interrupt: still polled; a pass through the main loop
      must stay shorter than one video frame or the song slows down
- [ ] Spectrum-style display, master scope from the mixer read-back

## M6 - SID spike

Test files at hand: Commando (PSID, `$5000-$5FC6`, 19 songs), Katakis
(PSID, `$0700-$70C2`, 34 songs), Great Giana Sisters (RSID, out of the
initial scope). All three are PAL, 6581, one SID.

- [x] Free program memory first: `-Oz` for non-critical code, pattern view
      without its 1 KB buffer
- [x] PSID header parser with host unit tests
- [x] Load a tune into bank 4, call init and play through the trampoline at
      `$CF00`; Commando and Katakis match a reference 6502 run register for
      register in Xemu (`tools/sidref`)
- [x] Zero page and stack: the tune gets its own (block 0 mapped)
- [x] CPU speed: tunes run at 40.5 MHz; no problems with the test tunes
- [x] Heard on real hardware (14 Nov 2026): Commando and Katakis correct,
      song change, pause/resume and MOD after SID all work
- [ ] Decide the register shadow question for visualisation
- [ ] Mixer setup so one-SID tunes are centred

## M7 - SID integration

- [x] SID files in the file browser; tune information panel
- [x] Sub-tune selection (cursor left/right), PAL/NTSC timing, SID model
- [x] Clear message for unsupported tunes (RSID, play address 0, tune in
      `$C000-$DFFF`, unsupported second SID address)
- [x] Pause without losing the tune's volume setting
- [ ] 2SID and 3SID tunes: allowed for `$D420/$D440/$D460`, untested
- [ ] Visualisers for SID (master scope from the mixer read-back)
- [ ] Program memory: only about 800 bytes left; must be freed before the
      SID visualisers

## M8 - Polish and release

- [ ] Playlist and automatic advance to the next file
- [ ] D81 and floppy file access
- [ ] Optional mouse support
- [ ] D81 release image built by CI
- [ ] User documentation
