"""Traces a controller silhouette from a reference PNG (pure Python PNG decoder).

The outline points in src/main.cpp (g_TopAndSide, g_UnderEdge, g_FaceEdge) were made with this
from a 1280x916 front-view picture of the controller on a white background (not included).
Prints the dark outline crossings per column for the left half (the pad is symmetric).
Usage: python tools/trace_outline.py <image.png> [ascii]
"""
import struct, sys, zlib, os

def load_png(path):
    data = open(path, 'rb').read()
    pos, idat, w = 8, b'', 0
    while pos < len(data):
        n, kind = struct.unpack('>I4s', data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + n]
        if kind == b'IHDR':
            w, h, depth, ctype = struct.unpack('>IIBB', body[:10])
        elif kind == b'IDAT':
            idat += body
        pos += 12 + n
    assert depth == 8 and ctype in (2, 6), (depth, ctype)
    bpp = 4 if ctype == 6 else 3
    raw = zlib.decompress(idat)
    stride = w * bpp
    rows, prev = [], bytearray(stride)
    for y in range(h):
        f = raw[y * (stride + 1)]
        line = bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for i in range(stride):
            a = line[i - bpp] if i >= bpp else 0
            b = prev[i]
            c = prev[i - bpp] if i >= bpp else 0
            if f == 1: line[i] = (line[i] + a) & 255
            elif f == 2: line[i] = (line[i] + b) & 255
            elif f == 3: line[i] = (line[i] + (a + b) // 2) & 255
            elif f == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[i] = (line[i] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        rows.append(line)
        prev = line
    return w, h, bpp, rows

if len(sys.argv) < 2:
    sys.exit(__doc__)
w, h, bpp, rows = load_png(sys.argv[1])

def solid(x, y):
    p = rows[y][x * bpp:(x + 1) * bpp]
    if bpp == 4 and p[3] < 128:
        return False
    return not (p[0] > 245 and p[1] > 245 and p[2] > 245)

if len(sys.argv) > 2 and sys.argv[2] == 'ascii':
    for y in range(0, h, 12):
        print(''.join('#' if solid(x, y) else '.' for x in range(0, w, 8)))
    sys.exit()

def dark(x, y):
    p = rows[y][x * bpp:(x + 1) * bpp]
    return (bpp == 3 or p[3] >= 128) and p[0] < 60 and p[1] < 60 and p[2] < 60

# Per column (left half; the pad is symmetric): every dark outline run from top to bottom.
for x in list(range(0, 640, 16)) + [640]:
    out, start = [], None
    for y in range(h):
        if dark(x, y) and start is None:
            start = y
        elif not dark(x, y) and start is not None:
            out.append(start)
            start = None
    print(x, out)
