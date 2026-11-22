"""Capture the StickS3 display as a PNG over the serial CLI.

Usage: .venv/bin/python scripts/screenshot.py --port /dev/ttyACM0 [--out shot.png]

Wire format (host sends `shot\\n`). Log lines before the header are skipped:

    SHOT <w> <h> RGB565BE
    <h lines, each exactly 4*w hex digits, upper or lower case, one row top to
     bottom; each pixel is 4 hex digits = RGB565 big-endian (high byte first)>
    END

Any other line inside the frame, a wrong row length, non-hex, a premature END
or w/h outside 1..1024 is a FrameError. `ERR <message>` before the header aborts.
"""
import argparse
import re
import struct
import sys
import time
import zlib

from serial_link import FrameError, add_common_args, read_frame, request, run

HEADER = re.compile(r"^SHOT (\d+) (\d+) RGB565BE$")
HEX = re.compile(r"^[0-9A-Fa-f]*$")


def rgb565_to_rgb888(v):
    r5, g6, b5 = (v >> 11) & 0x1F, (v >> 5) & 0x3F, v & 0x1F
    return (r5 << 3) | (r5 >> 2), (g6 << 2) | (g6 >> 4), (b5 << 3) | (b5 >> 2)


def _begin(line):
    m = HEADER.match(line)
    if not m:
        return None
    w, h = int(m.group(1)), int(m.group(2))
    if not (1 <= w <= 1024 and 1 <= h <= 1024):
        raise FrameError(f"bad dimensions {w}x{h}")
    return w, h


def parse_shot(lines):
    """Return (w, h, rgb888 bytes) from an iterable of text lines."""
    dims = {}

    def begin(line):
        d = _begin(line)
        if d:
            dims["wh"] = d
        return d

    def check(i, line):
        w, h = dims["wh"]
        if i >= h:
            raise FrameError(f"row {i}: more than {h} rows")
        if len(line) != 4 * w or not HEX.match(line):
            raise FrameError(f"row {i}: expected {4 * w} hex digits, got {line[:40]!r}")

    (w, h), rows = read_frame(lines, begin, "END", check)
    if len(rows) != h:
        raise FrameError(f"premature END: got {len(rows)} of {h} rows")
    rgb = bytearray()
    for row in rows:
        for x in range(w):
            rgb.extend(rgb565_to_rgb888(int(row[4 * x:4 * x + 4], 16)))
    return w, h, bytes(rgb)


def _chunk(tag, data):
    return (struct.pack(">I", len(data)) + tag + data
            + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF))


def encode_png(w, h, rgb):
    raw = b"".join(b"\x00" + rgb[y * w * 3:(y + 1) * w * 3] for y in range(h))
    return (b"\x89PNG\r\n\x1a\n"
            + _chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
            + _chunk(b"IDAT", zlib.compress(raw, 9))
            + _chunk(b"IEND", b""))


def main(argv=None):
    ap = add_common_args(argparse.ArgumentParser(description=__doc__.splitlines()[0]), 20)
    args = ap.parse_args(argv)
    w, h, rgb = request(args, "shot", parse_shot)
    path = args.out or time.strftime("shot-%Y%m%d-%H%M%S.png")
    with open(path, "wb") as f:
        f.write(encode_png(w, h, rgb))
    print(f"{path} ({w}x{h})")
    return 0


if __name__ == "__main__":
    sys.exit(run(main, None))
