#!/usr/bin/env python3
"""Compare Modplay65's engine with the micromod reference, tick by tick.

usage: compare.py file.mod ...   (see README.md in this directory)
Periods may differ by up to 2 units (finetune and arpeggio rounding).
"""
import collections, os, subprocess, sys
MODTRACE = os.environ.get("MODTRACE", "build-tests/modtrace")
REFDUMP = os.environ.get("REFDUMP", "build-tests/refdump")
tol = 2
tp=tv=tpos=tt=0
for f in sys.argv[1:]:
    mine = subprocess.run([MODTRACE, '-t', f], capture_output=True, text=True).stdout.splitlines()
    ref = subprocess.run([REFDUMP, f], capture_output=True, text=True).stdout.splitlines()
    mine = mine[1:]  # micromod_initialise already runs the first tick
    n = min(len(mine), len(ref)); pos_bad = per_bad = vol_bad = trig_bad = 0
    byfx = collections.Counter(); examples = []
    for i in range(n):
        a, fx = mine[i].split(' | '); a = list(map(int, a.split())); fx = fx.split(); b = list(map(int, ref[i].split()))
        if a[:2] != b[:2]:
            pos_bad += 1
            if len(examples) < 4: examples.append(f'  tick {i}: position mine {a[:2]} ref {b[:2]}')
            if pos_bad > 20: break
            continue
        for c in range(4):
            pa, va, pb, vb = a[2+3*c], a[3+3*c], b[2+3*c], b[3+3*c]
            bad = None
            if a[4+3*c] >= 0 and a[4+3*c] != b[4+3*c]: trig_bad += 1; byfx['trigger offset fx '+fx[c][0]] += 1
            if abs(pa - pb) > tol and not (va == 0 and vb == 0): per_bad += 1; bad = 'period'
            elif va != vb and pa and pb: vol_bad += 1; bad = 'volume'
            if bad:
                byfx[bad + ' fx ' + fx[c][0] + (fx[c][1] if fx[c][0] == 'E' else '')] += 1
                if len(examples) < 6: examples.append(f'  tick {i} pos {a[:2]} ch{c} fx {fx[c]}: mine p{pa} v{va}  ref p{pb} v{vb}')
    print(f'{f.split("/")[-1]}: ticks mine {len(mine)} ref {len(ref)}; position diffs {pos_bad}, period diffs {per_bad}, volume diffs {vol_bad}, trigger offset diffs {trig_bad}')
    for k, v in byfx.most_common(8): print('   ', k, v)
    print('\n'.join(examples))
