"""Recolor the current handheld icon (with the plus on the screen) in PlayStation blue.

Run from any folder: python3 recolor.py
It reads android/app/src/main/res/mipmap/ic_launcher_plus.png and writes the five
ps-blue-*.png files next to this script.

Each pixel is a weighted mix of four source colors (screen and rim yellow, orange
ring, dark body, bottom edge). The weights come from the distance to the source
colors, so flat areas map exactly and anti-aliased edges blend. The plus fill is
found by a flood fill from the middle of the plus, and can get its own color.
"""
from collections import deque
from pathlib import Path

from pngio import read_png, write_png

HERE = Path(__file__).resolve().parent
SRC = HERE.parents[3] / 'android/app/src/main/res/mipmap/ic_launcher_plus.png'

# Colors in the source icon: screen and rim, orange ring, dark body, bottom edge.
ANCHORS = {'Y': (244, 190, 0), 'O': (228, 124, 0), 'P': (62, 8, 48), 'M': (180, 16, 78)}
PLUS_SEED = (292, 172)  # a pixel inside the plus fill


def hexc(value):
    value = value.lstrip('#')
    return tuple(int(value[i:i + 2], 16) for i in (0, 2, 4))


def lerp(a, b, t):
    return tuple(a[i] + (b[i] - a[i]) * t for i in range(3))


def dist2(p, q):
    return sum((p[i] - q[i]) ** 2 for i in range(3))


w, h, src = read_png(SRC)


def flood(seed):
    seen = bytearray(w * h)
    queue = deque([seed])
    seen[seed[1] * w + seed[0]] = 1
    while queue:
        x, y = queue.popleft()
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            xx, yy = x + dx, y + dy
            if 0 <= xx < w and 0 <= yy < h and not seen[yy * w + xx]:
                i = (yy * w + xx) * 4
                if src[i + 3] == 255 and dist2(tuple(src[i:i + 3]), ANCHORS['Y']) < 900:
                    seen[yy * w + xx] = 1
                    queue.append((xx, yy))
    return seen


def dilate(mask, rounds):
    out = bytearray(mask)
    for _ in range(rounds):
        nxt = bytearray(out)
        for y in range(1, h - 1):
            row = y * w
            for x in range(1, w - 1):
                if not out[row + x] and (out[row + x - 1] or out[row + x + 1] or out[row + x - w] or out[row + x + w]):
                    nxt[row + x] = 1
        out = nxt
    return out


# The fill plus a 2 pixel border, so that the anti-aliased edge gets the fill color too.
plus_area = dilate(flood(PLUS_SEED), 2)


def recolor(name, yellow, orange, body, edge, plus_fill=None, yellow_bottom=None):
    """Write one variant. yellow_bottom makes the yellow a vertical gradient."""
    out = bytearray(len(src))
    yo = [ANCHORS['Y'][j] - ANCHORS['O'][j] for j in range(3)]
    for y in range(h):
        targets = {'Y': hexc(yellow), 'O': hexc(orange), 'P': hexc(body), 'M': hexc(edge)}
        if yellow_bottom:
            targets['Y'] = lerp(hexc(yellow), hexc(yellow_bottom), y / (h - 1))
        for x in range(w):
            i = (y * w + x) * 4
            alpha = src[i + 3]
            if alpha == 0:
                continue
            p = tuple(src[i:i + 3])
            weights = {k: 1.0 / (dist2(p, v) + 4.0) ** 2 for k, v in ANCHORS.items()}
            total = sum(weights.values())
            color = [0.0, 0.0, 0.0]
            for k, wt in weights.items():
                for j in range(3):
                    color[j] += targets[k][j] * wt / total
            if plus_fill and plus_area[y * w + x]:
                # Share of yellow between the orange ring and the yellow fill.
                po = [p[j] - ANCHORS['O'][j] for j in range(3)]
                t = max(0.0, min(1.0, sum(po[j] * yo[j] for j in range(3)) / sum(v * v for v in yo)))
                color = list(lerp(targets['O'], hexc(plus_fill), t))
            out[i:i + 3] = bytes(int(max(0, min(255, round(v)))) for v in color)
            out[i + 3] = alpha
    write_png(HERE / name, w, h, out)
    print('wrote', name)


if __name__ == '__main__':
    # PlayStation blue: bright #0070D1, dark #003791.
    recolor('ps-blue-1-classic.png', '#0070D1', '#003791', '#0A1633', '#00235E')
    recolor('ps-blue-2-white-plus.png', '#0070D1', '#003791', '#0A1633', '#00235E', plus_fill='#FFFFFF')
    recolor('ps-blue-3-ice-screen.png', '#CFE8FF', '#0070D1', '#001A4D', '#0050A8', plus_fill='#0070D1')
    recolor('ps-blue-4-cyan-accent.png', '#00A8E8', '#0070D1', '#00143D', '#002A7A', plus_fill='#FFFFFF')
    recolor('ps-blue-5-gradient-white-plus.png', '#4DB8FF', '#0058B8', '#0B1F4D', '#002B7F', plus_fill='#FFFFFF', yellow_bottom='#0058B8')
