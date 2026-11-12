# Hardware notes

Register facts collected for Modplay65. Addresses and bit names come from
`iomap.txt` in the mega65-core repository unless stated otherwise. Behaviour
marked **[verify]** has not been tested by this project yet.

## Audio DMA

Four channels, 16 register bytes each.

| Channel | Base | Native side | Cross-feed volume |
| --- | --- | --- | --- |
| 0 | `$D720` | A | `$D71C` (CH0RVOL) |
| 1 | `$D730` | A | `$D71D` (CH1RVOL) |
| 2 | `$D740` | B | `$D71E` (CH2LVOL) |
| 3 | `$D750` | B | `$D71F` (CH3LVOL) |

Side A is left, side B is right: channels 0 and 1 play left, channels 2
and 3 play right. This follows from the core source (`gs4510.vhdl`: left =
ch0 + ch1 + cross-feed of ch2/3, right = ch2 + ch3 + cross-feed of ch0/1)
and was confirmed by ear on real hardware with each channel played alone
(12 Nov 2026). SID 1 (`$D400`) was heard left, SID 3 (`$D440`) right.

### Per-channel registers (offset from channel base)

| Offset | Name | Meaning |
| --- | --- | --- |
| `+0` | control | bit 7 enable, bit 6 loop, bit 5 unsigned samples, bit 4 play built-in 32-sample sine, bit 3 stop flag, bits 1-0 sample format (`11` 16-bit, `10` 8-bit, `01` upper nybble, `00` lower nybble) |
| `+1..+3` | base address | 24-bit start address |
| `+4..+6` | frequency | 24-bit increment |
| `+7..+8` | top address | 16-bit end address (low 16 bits only) |
| `+9` | volume | playback volume |
| `+A..+C` | current address | 24-bit playhead |
| `+D..+F` | timing counter | 24-bit phase accumulator |

### Global registers

| Address | Bits | Meaning |
| --- | --- | --- |
| `$D711` | 7 | AUDEN, enable audio DMA |
| `$D711` | 4 | NOMIX, audio DMA bypasses the mixer |
| `$D711` | 3 | PWM/PDM output encoding select |
| `$D712` | 0 | SATURATE |
| `$D712` | 1 | CHANSWAP |
| `$D713` | 3-0 / 7-4 | per-channel interrupt enable / flag |
| `$D714-$D717` | | per-channel interrupt address offset (bits 15-8) |

### Frequency **[verify]**

```
FREQ = sample_rate_hz * 2^24 / 40500000
```

Examples: 8287 Hz (Amiga C-2, period 428) → 3433; 14080 Hz → 5833 (`$0016C9`).
For an Amiga period `p`: `FREQ ≈ 1469309 / p`.

### Limits

- Sample data must be in the first 384 KB of memory (source: MEGA65 Digest
  article, see ARCHITECTURE.md).
- One playback window is at most 64 KB because the top address is 16 bits.

### Xemu behaviour (checked in the Xemu source, Nov 2026)

- Xemu does not emulate the built-in sine wave (control bit 4): it plays
  the RAM window instead. The test tone therefore uses a real 32-byte sine
  sample in bank 4.
- Xemu restarts a looping channel at the base address when the low 16 bits
  of the playhead equal the top address.
- Verified in Xemu: with a looped 32-byte sample at `$040000` the playhead
  (`+A..+C`) stays inside `$040000-$04001F`.

### Playback recipes

- One-shot: base = start, top = low 16 bits of the end, current address =
  start, control = enable + 8-bit. In Xemu the channel sets the stop flag
  (bit 3) at the end.
- Play once, then loop a tail: start as a looping window from the sample
  start to the loop end, then immediately rewrite the base address to the
  loop start. The base is only read when the playhead wraps. Verified in
  Xemu and confirmed by ear on real hardware (12 Nov 2026).
- Looped 32-byte sine sample confirmed audible on real hardware
  (12 Nov 2026), both sides.

## SID

| Chip | Address |
| --- | --- |
| SID 1 | `$D400` |
| SID 2 | `$D420` |
| SID 3 | `$D440` |
| SID 4 | `$D460` |

- `$D63C` bits 3-0: SIDMODE, 0 = 6581, 1 = 8580; presumably one bit per
  chip **[verify]**.
- `$D63C` bit 4: enable extra SID registers.

## Audio mixer and read-back

- `$D6F4`: mixer register select; `$D6F5`: mixer register data.
- `$D6FC/$D6FD`: audio read-back, low/high byte, source selected by `$D6F4`.
- `$D6F8-$D6FB`: 16-bit digital audio output registers, left and right.

## Video timing

- `$D06F` bit 7: PALNTSC, set = NTSC timing.

## Keyboard

- `$D610`: ASCII code of the oldest queued key, 0 if none; write any value
  to remove it from the queue. Needs neither KERNAL nor interrupts.

## Toolchain behaviour

- The llvm-mos MEGA65 startup code executes `SEI` and switches ROMs out
  before `main()`; interrupts stay disabled until the program exits. KERNAL
  services that depend on the system interrupt (GETIN keyboard input, the
  jiffy clock) therefore do not work. Modplay65 reads the keyboard through
  `$D610` and will install its own interrupt handlers.

- Observed on Xemu/ROM 920413 (Nov 2026): `printf` through KERNAL CHROUT
  printed lines out of order, dropped text, did not format `%lu`, and the
  program then left its key loop by itself. Reproduced in a headless Xemu
  run with the same ROM. Cause not investigated;
  Modplay65 avoids KERNAL calls and stdio altogether and writes to screen
  RAM directly (`src/hw/screen.c`).
- `main()` returning does not go back to BASIC: the SDK's exit code loops
  forever.

## Misc

- `$D02F`: write `$47` then `$53` to enable MEGA65 I/O.
- `$D770-$D77F`: hardware multiplier, `$D768-$D76F`: divider output.
