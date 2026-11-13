# SID reference run

`sidref.py` runs a PSID tune in a minimal 6502 emulator written for this
project (legal NMOS opcodes only, no SID sound) and prints the SID register
state ($D400-$D418) after every play call:

```sh
python3 tools/sidref/sidref.py tune.sid [song] [calls] > ref.txt
```

Used to check the MEGA65 player: a SID register snapshot taken in Xemu
while Modplay65 plays the tune must appear, all 25 bytes, somewhere in this
sequence, and the call numbers of successive snapshots show the play rate.
Snapshots come from Xemu's monitor command `sidregs`, which is a local
addition to Xemu, not part of it.

Limits: a tune that reads oscillator 3, the envelope of voice 3, timers or
the raster line will differ from the real machine.
