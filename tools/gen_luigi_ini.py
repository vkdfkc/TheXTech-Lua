#!/usr/bin/env python
"""Extract Luigi frame offsets + physics from source and write INI files (states 1-11)."""

import re, os
from collections import defaultdict

with open('e:/xtech-source/src/main/player_frames.cpp') as f:
    text = f.read()

lx = {}
ly = {}

def ev(expr):
    expr = expr.strip().rstrip(';').strip()
    while True:
        m = re.search(r'LuigiFrame([XY])\[(\d+)\]', expr)
        if not m:
            break
        arr = lx if m.group(1) == 'X' else ly
        val = arr.get(int(m.group(2)), 0)
        expr = expr[:m.start()] + str(val) + expr[m.end():]
    try:
        return int(eval(expr))
    except:
        return 0

# Parse all = assignments
for m in re.finditer(r'LuigiFrame([XY])\[(\d+)\]\s*=\s*([^;]+);', text):
    arr = 'X' if m.group(1) == 'X' else 'Y'
    idx = int(m.group(2))
    val = ev(m.group(3))
    if arr == 'X':
        lx[idx] = val
    else:
        ly[idx] = val

# Parse -= assignments
for m in re.finditer(r'LuigiFrame([XY])\[(\d+)\]\s*-=\s*(\d+);', text):
    arr = 'X' if m.group(1) == 'X' else 'Y'
    idx = int(m.group(2))
    val = int(m.group(3))
    if arr == 'X':
        lx[idx] = lx.get(idx, 0) - val
    else:
        ly[idx] = ly.get(idx, 0) - val

# Bulk -= 2 for A = 1..maxPlayerFrames (1150)
MAX_PF = 1150  # 100 * 11 + 50
for a in range(1, MAX_PF + 1):
    lx[a] = lx.get(a, 0) - 2
    ly[a] = ly.get(a, 0) - 2

# Specific adjustments after bulk (lines 487-494): Y -= 2 for these indices
for idx in [101, 102, 105, 106, 99, 98, 95, 94]:
    ly[idx] = ly.get(idx, 0) - 2

# Physics for Luigi (character index 2, state 1 = Small, 2-11 = Big)
# From setup_physics.cpp lines 50-61
physics = {
    1: {'width': 24, 'height': 30, 'grab-offset-x': 16, 'grab-offset-y': -4},
}
for s in range(2, 12):
    physics[s] = {'width': 24, 'height': 60, 'height-duck': 30, 'grab-offset-x': 18, 'grab-offset-y': 16}

outdir = "E:/thextech-super-mario-bros-x/worlds/vk's world2"

for state in range(1, 12):
    phys = physics[state]
    entries = []
    for x in range(10):
        for y in range(10):
            sprite_idx = y + 10 * x - 49
            a = state * 100 + sprite_idx
            ox = lx.get(a, 0)
            oy = ly.get(a, 0)
            # Stored value = -offset; INI offset = -stored
            if ox != 0 or oy != 0:
                entries.append((x, y, -ox, -oy))

    fname = os.path.join(outdir, 'luigi-%d.ini' % state)
    with open(fname, 'w') as f:
        f.write('; Luigi State %d frame offsets\n\n' % state)
        f.write('[common]\n')
        for key in ['width', 'height', 'height-duck', 'grab-offset-x', 'grab-offset-y']:
            if key in phys:
                f.write('%s = %d\n' % (key, phys[key]))
        f.write('\n')
        for x, y, ox, oy in entries:
            f.write('[frame-%d-%d]\n' % (x, y))
            f.write('offsetX = %d\n' % ox)
            f.write('offsetY = %d\n\n' % oy)
    print('%s: %d frames' % (fname, len(entries)))

print('Done - %d files' % 11)
