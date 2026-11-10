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

Going by the register names, side A is left and side B is right **[verify]**.

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

## Misc

- `$D02F`: write `$47` then `$53` to enable MEGA65 I/O.
- `$D770-$D77F`: hardware multiplier, `$D768-$D76F`: divider output.
