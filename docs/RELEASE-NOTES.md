# Modplay65 0.2.0

First release. Modplay65 plays MOD and SID files from the SD card of a
MEGA65, with a file browser and sound visualisation.

## Contents

- `modplay65.d81`: disk image with the program. Copy it to the SD card,
  mount it, then `DLOAD "MODPLAY65"` and `RUN`.
- `modplay65.prg`: the program alone, e.g. for `m65 -r` or Xemu.

Copy your MOD and SID files anywhere onto the SD card; they are not part
of the image. See the README for the keys and limits.

## Features

- 4-channel MOD files (15 and 31 instruments), all common ProTracker
  effects, up to about 182 KB of samples at full rate; larger modules
  play with the largest samples at reduced rate.
- PSID tunes with one or two SIDs, sub-tune selection, PAL/NTSC, VBI and
  CIA timing (also multi-speed tunes).
- File browser with sub-directories and long file names.
- Oscilloscopes, pattern view, sample list and level meters for MOD;
  voice display with note, waveform and envelope for SID.
- Volume and stereo width for MOD and SID; mono SID tunes centred.
- Automatic advance to the next file; SID time limit (key L).

SID volume, centring and the voice display need MEGA65 core 0.97 or
newer.

## Known limits

RSID tunes and tunes in `$C000-$DFFF` are refused; 3SID tunes are
untested; D81 images and floppies cannot be browsed for music yet.
