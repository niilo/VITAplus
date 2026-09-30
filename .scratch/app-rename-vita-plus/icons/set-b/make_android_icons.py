"""Build the Android icon files from ps-blue-3-ice-screen.png (the chosen VITA+ icon).

Run from any folder: python3 make_android_icons.py
It writes into android/app/src/main/res:
  mipmap-<density>/ic_launcher_foreground.png   adaptive icon foreground, picture at 72 percent
  mipmap/ic_launcher.png, mipmap/ic_launcher_plus.png   the picture itself (512 x 512)
  drawable-<density>/ic_stat_vita.png           white notification icon with the plus cut out
The background color and the adaptive icon file are written by hand (see issue 03).
"""
from collections import deque
from pathlib import Path

from pngio import read_png, write_png

HERE = Path(__file__).resolve().parent
RES = HERE.parents[3] / 'android/app/src/main/res'
SRC = HERE / 'ps-blue-3-ice-screen.png'

DENSITIES = {'mdpi': 1.0, 'hdpi': 1.5, 'xhdpi': 2.0, 'xxhdpi': 3.0, 'xxxhdpi': 4.0}
FOREGROUND_SHARE = 0.72  # share of the 108 dp canvas that the picture takes


def weights(src_len, dst_len):
    """Area weights: for each destination pixel, a list of (source index, weight)."""
    scale = src_len / dst_len
    out = []
    for d in range(dst_len):
        a, b = d * scale, (d + 1) * scale
        taps = []
        i = int(a)
        while i < b and i < src_len:
            lo, hi = max(a, i), min(b, i + 1)
            if hi > lo:
                taps.append((i, (hi - lo) / scale))
            i += 1
        out.append(taps)
    return out


def resize(px, sw, sh, dw, dh):
    """Area-average resize of straight-alpha RGBA, done on premultiplied values."""
    wx, wy = weights(sw, dw), weights(sh, dh)
    # horizontal pass
    mid = [[0.0] * (dw * 4) for _ in range(sh)]
    for y in range(sh):
        row = y * sw * 4
        for x, taps in enumerate(wx):
            r = g = b = a = 0.0
            for i, wt in taps:
                k = row + i * 4
                al = px[k + 3] / 255.0
                r += px[k] * al * wt
                g += px[k + 1] * al * wt
                b += px[k + 2] * al * wt
                a += al * wt
            mid[y][x*4:x*4+4] = [r, g, b, a]
    out = bytearray(dw * dh * 4)
    for y, taps in enumerate(wy):
        for x in range(dw):
            r = g = b = a = 0.0
            for j, wt in taps:
                m = mid[j]
                k = x * 4
                r += m[k] * wt
                g += m[k + 1] * wt
                b += m[k + 2] * wt
                a += m[k + 3] * wt
            o = (y * dw + x) * 4
            if a > 0:
                out[o:o+3] = bytes(min(255, round(v / a)) for v in (r, g, b))
            out[o + 3] = min(255, round(a * 255))
    return out


def place(small, sw, sh, cw, ch):
    """Center a small RGBA image on a transparent canvas."""
    out = bytearray(cw * ch * 4)
    ox, oy = (cw - sw) // 2, (ch - sh) // 2
    for y in range(sh):
        s = y * sw * 4
        d = ((y + oy) * cw + ox) * 4
        out[d:d + sw * 4] = small[s:s + sw * 4]
    return out


w, h, src = read_png(SRC)
assert (w, h) == (512, 512)

# adaptive icon foregrounds
for name, scale in DENSITIES.items():
    canvas = round(108 * scale)
    pic = round(canvas * FOREGROUND_SHARE)
    fg = place(resize(src, w, h, pic, pic), pic, pic, canvas, canvas)
    folder = RES / f'mipmap-{name}'
    folder.mkdir(parents=True, exist_ok=True)
    write_png(folder / 'ic_launcher_foreground.png', canvas, canvas, fg)

# the picture itself, for the in-app uses and as the default launcher resource
for name in ('ic_launcher.png', 'ic_launcher_plus.png'):
    write_png(RES / 'mipmap' / name, w, h, src)

# notification icon: white silhouette, the plus cut out
def near(p, q, limit):
    return sum((p[i] - q[i]) ** 2 for i in range(3)) < limit

PLUS_BLUE, SEED = (0, 112, 209), (292, 172)
sil = bytearray(len(src))
for i in range(0, len(src), 4):
    sil[i:i+3] = b'\xff\xff\xff'
    sil[i + 3] = src[i + 3]
seen = bytearray(w * h)
queue = deque([SEED])
seen[SEED[1] * w + SEED[0]] = 1
while queue:
    x, y = queue.popleft()
    sil[(y * w + x) * 4 + 3] = 0
    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        xx, yy = x + dx, y + dy
        if 0 <= xx < w and 0 <= yy < h and not seen[yy * w + xx]:
            k = (yy * w + xx) * 4
            if near(tuple(src[k:k+3]), PLUS_BLUE, 2500):
                seen[yy * w + xx] = 1
                queue.append((xx, yy))
for name, scale in DENSITIES.items():
    size = round(24 * scale)
    folder = RES / f'drawable-{name}'
    folder.mkdir(parents=True, exist_ok=True)
    write_png(folder / 'ic_stat_vita.png', size, size, resize(sil, w, h, size, size))
print('done')
