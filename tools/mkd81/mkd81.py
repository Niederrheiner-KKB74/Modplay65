#!/usr/bin/env python3
"""Create a 1581 (D81) disk image with PRG files.

Usage: mkd81.py OUT.d81 "DISK NAME,ID" FILE[=NAME] ...

Each FILE is a PRG file (load address included); NAME is the CBM file
name, default the file's base name without extension, upper case. Only
what Modplay65 needs: PRG files, one directory sector (up to 8 files).
"""
import os
import sys

TRACKS, SECTORS = 80, 40
DIR_TRACK = 40


def offset(track, sector):
    return ((track - 1) * SECTORS + sector) * 256


def petscii_name(name, length=16):
    data = name.upper().encode('ascii')[:length]
    return data + b'\xa0' * (length - len(data))


def make_d81(files, disk_name, disk_id):
    img = bytearray(TRACKS * SECTORS * 256)
    free = {t: set(range(SECTORS)) for t in range(1, TRACKS + 1)}
    free[DIR_TRACK] -= {0, 1, 2, 3}

    # header sector 40/0
    h = offset(DIR_TRACK, 0)
    img[h:h + 3] = bytes([DIR_TRACK, 3, 0x44])
    img[h + 4:h + 0x14] = petscii_name(disk_name)
    img[h + 0x14:h + 0x16] = b'\xa0\xa0'
    img[h + 0x16:h + 0x18] = disk_id
    img[h + 0x18:h + 0x1d] = b'\xa0' + b'3D' + b'\xa0\xa0'

    # file data: tracks 39 down to 1, then 41 up to 80
    order = list(range(DIR_TRACK - 1, 0, -1)) + list(range(DIR_TRACK + 1, TRACKS + 1))

    def allocate():
        for t in order:
            if free[t]:
                s = min(free[t])
                free[t].remove(s)
                return t, s
        raise SystemExit('disk full')

    entries = []
    for cbm_name, data in files:
        chunks = [data[i:i + 254] for i in range(0, len(data), 254)] or [b'']
        sectors = [allocate() for _ in chunks]
        for i, chunk in enumerate(chunks):
            o = offset(*sectors[i])
            if i + 1 < len(chunks):
                img[o:o + 2] = bytes(sectors[i + 1])
            else:
                img[o:o + 2] = bytes([0, len(chunk) + 1])
            img[o + 2:o + 2 + len(chunk)] = chunk
        entries.append((cbm_name, sectors[0], len(chunks)))

    if len(entries) > 8:
        raise SystemExit('more than 8 files are not supported')
    d = offset(DIR_TRACK, 3)
    img[d:d + 2] = b'\x00\xff'
    for i, (cbm_name, (t, s), blocks) in enumerate(entries):
        e = d + i * 32
        img[e + 2:e + 5] = bytes([0x82, t, s])
        img[e + 5:e + 21] = petscii_name(cbm_name)
        img[e + 30:e + 32] = blocks.to_bytes(2, 'little')

    # BAM sectors 40/1 (tracks 1-40) and 40/2 (tracks 41-80)
    for n, first in ((1, 1), (2, 41)):
        b = offset(DIR_TRACK, n)
        img[b:b + 2] = bytes([DIR_TRACK, 2]) if n == 1 else b'\x00\xff'
        img[b + 2:b + 8] = bytes([0x44, 0xBB]) + disk_id + bytes([0xC0, 0])
        for t in range(first, first + 40):
            e = b + 0x10 + (t - first) * 6
            bits = sum(1 << s for s in free[t])
            img[e] = len(free[t])
            img[e + 1:e + 6] = bits.to_bytes(5, 'little')
    return img


def main(argv):
    if len(argv) < 4:
        raise SystemExit(__doc__)
    out, label = argv[1], argv[2]
    disk_name, _, disk_id = label.partition(',')
    disk_id = (disk_id or '65').upper().encode('ascii')[:2].ljust(2, b' ')
    files = []
    for spec in argv[3:]:
        path, _, name = spec.partition('=')
        if not name:
            name = os.path.splitext(os.path.basename(path))[0]
        with open(path, 'rb') as f:
            files.append((name, f.read()))
    with open(out, 'wb') as f:
        f.write(make_d81(files, disk_name, disk_id))


if __name__ == '__main__':
    main(sys.argv)
