#!/usr/bin/env python3
"""Unit test for fps_sample.py. Run: python3 tools/android/test_fps_sample.py"""

import struct
import unittest
import zlib

import fps_sample


def paeth(a, b, c):
    pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
    return a if pa <= pb and pa <= pc else (b if pb <= pc else c)


def encode_rgb(width, height, pixels, filters):
    """Make an RGB PNG. pixels is a list of rows of (r, g, b). filters has one filter type per row."""
    bpp = 3
    raw = bytearray()
    previous = bytearray(width * bpp)
    for y in range(height):
        line = bytearray(v for p in pixels[y] for v in p)
        kind = filters[y]
        out = bytearray()
        for i, value in enumerate(line):
            a = line[i - bpp] if i >= bpp else 0
            b = previous[i]
            c = previous[i - bpp] if i >= bpp else 0
            predictor = [0, a, b, (a + b) >> 1, paeth(a, b, c)][kind]
            out.append((value - predictor) & 255)
        raw.append(kind)
        raw += out
        previous = line

    def chunk(name, body):
        return struct.pack(">I", len(body)) + name + body + struct.pack(">I", zlib.crc32(name + body) & 0xFFFFFFFF)

    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(bytes(raw))) + chunk(b"IEND", b""))


class PngTest(unittest.TestCase):
    def test_all_filter_types(self):
        width, height = 7, 5
        pixels = [[((x * 37 + y * 11) % 256, (x * 5 + y * 71) % 256, (x * y * 13) % 256) for x in range(width)]
                  for y in range(height)]
        data = encode_rgb(width, height, pixels, [0, 1, 2, 3, 4])
        w, h, rgba = fps_sample.read_png(data)
        self.assertEqual((w, h), (width, height))
        for y in range(height):
            for x in range(width):
                o = (y * width + x) * 4
                self.assertEqual(tuple(rgba[o:o + 3]), pixels[y][x])
                self.assertEqual(rgba[o + 3], 255)

    def test_write_then_read(self):
        rgba = bytearray()
        for i in range(6 * 4):
            rgba += bytes([i, 255 - i, i * 3 % 256, 200])
        import os
        import tempfile
        path = os.path.join(tempfile.mkdtemp(), "x.png")
        fps_sample.write_png(path, 6, 4, rgba)
        with open(path, "rb") as f:
            w, h, back = fps_sample.read_png(f.read())
        self.assertEqual((w, h, bytes(back)), (6, 4, bytes(rgba)))

    def test_crop_scales_with_the_width(self):
        width, height = 1280, 720
        rgba = bytearray(width * height * 4)
        cw, ch, out = fps_sample.crop_counter(width, height, rgba)
        self.assertEqual((cw, ch), (165, 50))
        self.assertEqual(len(out), cw * ch * 4)

    def test_stack_needs_the_same_size(self):
        a = (2, 1, bytes(8))
        b = (2, 1, bytes(8))
        self.assertEqual(fps_sample.stack([a, b])[:2], (2, 2))
        with self.assertRaises(ValueError):
            fps_sample.stack([a, (3, 1, bytes(12))])

    def test_rejects_other_files(self):
        with self.assertRaises(ValueError):
            fps_sample.read_png(b"not a png at all")


if __name__ == "__main__":
    unittest.main()
