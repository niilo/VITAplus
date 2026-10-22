#!/usr/bin/env python3
"""Crop the FPS counter from screenshots and stack the crops into one PNG.

Usage: tools/android/fps_sample.py <out.png> [--count 6] [--interval 2]
       tools/android/fps_sample.py <out.png> --png shot1.png shot2.png ...

Without --png it takes --count screenshots with adb, --interval seconds apart.
The counter is in the top left corner of the screen. The crop is 330 x 100
pixels at a screen width of 2560 and scales with the screen. Read the numbers
from the result: one picture holds all samples.

The program reads 8-bit PNG files without interlacing (RGB or RGBA), which is
what `screencap -p` writes. It needs only the Python standard library.
"""

import argparse
import struct
import subprocess
import sys
import time
import zlib

CROP_WIDTH = 330
CROP_HEIGHT = 100
REFERENCE_WIDTH = 2560


def read_png(data):
    """Return (width, height, rgba) of a PNG file. rgba is a bytearray."""
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("not a PNG file")
    pos, idat, header = 8, b"", None
    while pos < len(data):
        length, kind = struct.unpack(">I4s", data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + length]
        if kind == b"IHDR":
            header = struct.unpack(">IIBBBBB", body)
        elif kind == b"IDAT":
            idat += body
        pos += 12 + length
    if header is None:
        raise ValueError("no IHDR chunk")
    width, height, depth, color, _, _, interlace = header
    if depth != 8 or color not in (2, 6) or interlace != 0:
        raise ValueError("only 8-bit RGB or RGBA without interlacing is supported")
    bpp = 4 if color == 6 else 3
    stride = width * bpp
    raw = zlib.decompress(idat)
    rows = []
    previous = bytearray(stride)
    pos = 0
    for _ in range(height):
        kind = raw[pos]
        line = bytearray(raw[pos + 1:pos + 1 + stride])
        pos += 1 + stride
        if kind == 1:
            for i in range(bpp, stride):
                line[i] = (line[i] + line[i - bpp]) & 255
        elif kind == 2:
            for i in range(stride):
                line[i] = (line[i] + previous[i]) & 255
        elif kind == 3:
            for i in range(stride):
                left = line[i - bpp] if i >= bpp else 0
                line[i] = (line[i] + ((left + previous[i]) >> 1)) & 255
        elif kind == 4:
            for i in range(stride):
                a = line[i - bpp] if i >= bpp else 0
                b = previous[i]
                c = previous[i - bpp] if i >= bpp else 0
                pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
                predictor = a if pa <= pb and pa <= pc else (b if pb <= pc else c)
                line[i] = (line[i] + predictor) & 255
        elif kind != 0:
            raise ValueError("unknown filter type %d" % kind)
        rows.append(line)
        previous = line
    rgba = bytearray(width * height * 4)
    for y, line in enumerate(rows):
        for x in range(width):
            o = (y * width + x) * 4
            rgba[o:o + 3] = line[x * bpp:x * bpp + 3]
            rgba[o + 3] = line[x * 4 + 3] if bpp == 4 else 255
    return width, height, rgba


def write_png(path, width, height, rgba):
    def chunk(kind, body):
        return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body) & 0xFFFFFFFF)

    raw = bytearray()
    for y in range(height):
        raw.append(0)
        raw += rgba[y * width * 4:(y + 1) * width * 4]
    png = (b"\x89PNG\r\n\x1a\n"
           + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
           + chunk(b"IDAT", zlib.compress(bytes(raw), 9))
           + chunk(b"IEND", b""))
    with open(path, "wb") as f:
        f.write(png)


def crop_counter(width, height, rgba):
    """Return (crop_width, crop_height, rgba) of the top left corner."""
    scale = width / REFERENCE_WIDTH
    cw = min(width, max(1, round(CROP_WIDTH * scale)))
    ch = min(height, max(1, round(CROP_HEIGHT * scale)))
    out = bytearray()
    for y in range(ch):
        out += rgba[y * width * 4:(y * width + cw) * 4]
    return cw, ch, out


def stack(crops):
    """Stack crops of the same size on top of each other."""
    cw, ch, _ = crops[0]
    if any((c[0], c[1]) != (cw, ch) for c in crops):
        raise ValueError("the screenshots have different sizes")
    return cw, ch * len(crops), b"".join(bytes(c[2]) for c in crops)


def screenshot():
    result = subprocess.run(["adb", "exec-out", "screencap", "-p"], capture_output=True, check=True)
    return result.stdout


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("out")
    parser.add_argument("--count", type=int, default=6)
    parser.add_argument("--interval", type=float, default=2.0)
    parser.add_argument("--png", nargs="+", help="use these screenshots instead of adb")
    args = parser.parse_args()

    crops = []
    if args.png:
        for path in args.png:
            with open(path, "rb") as f:
                crops.append(crop_counter(*read_png(f.read())))
    else:
        for i in range(args.count):
            crops.append(crop_counter(*read_png(screenshot())))
            if i + 1 < args.count:
                time.sleep(args.interval)
    width, height, rgba = stack(crops)
    write_png(args.out, width, height, rgba)
    print(args.out)


if __name__ == "__main__":
    sys.exit(main())
