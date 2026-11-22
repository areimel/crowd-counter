import struct
import sys
import unittest
import zlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import export_csv  # noqa: E402
import screenshot  # noqa: E402
import serial_link as sl  # noqa: E402


def decode_png(data):
    assert data[:8] == b"\x89PNG\r\n\x1a\n"
    pos, chunks = 8, []
    while pos < len(data):
        n, = struct.unpack(">I", data[pos:pos + 4])
        tag, body = data[pos + 4:pos + 8], data[pos + 8:pos + 8 + n]
        crc, = struct.unpack(">I", data[pos + 8 + n:pos + 12 + n])
        assert crc == zlib.crc32(tag + body) & 0xFFFFFFFF, tag
        chunks.append((tag, body))
        pos += 12 + n
    return chunks


class FakeSerial:
    def __init__(self, chunks):
        self.chunks = list(chunks)
        self.in_waiting = 0

    def read(self, n=1):
        if not self.chunks:
            self.in_waiting = 0
            return b""
        data = self.chunks.pop(0)
        self.in_waiting = len(self.chunks[0]) if self.chunks else 0
        return data


class PngTests(unittest.TestCase):
    def test_colour_conversion(self):
        c = screenshot.rgb565_to_rgb888
        self.assertEqual(c(0xF800), (255, 0, 0))
        self.assertEqual(c(0x07E0), (0, 255, 0))
        self.assertEqual(c(0x001F), (0, 0, 255))
        self.assertEqual(c(0xFFFF), (255, 255, 255))
        self.assertEqual(c(0x0000), (0, 0, 0))

    def test_encode_round_trip(self):
        rgb = b"".join(bytes(screenshot.rgb565_to_rgb888(v)) for v in (0xF800, 0x07E0, 0x001F, 0xFFFF))
        chunks = decode_png(screenshot.encode_png(2, 2, rgb))
        self.assertEqual([t for t, _ in chunks], [b"IHDR", b"IDAT", b"IEND"])
        self.assertEqual(struct.unpack(">IIBBBBB", chunks[0][1]), (2, 2, 8, 2, 0, 0, 0))
        raw = zlib.decompress(chunks[1][1])
        self.assertEqual(raw, b"\x00\xff\x00\x00\x00\xff\x00" + b"\x00\x00\x00\xff\xff\xff\xff")


class ShotParseTests(unittest.TestCase):
    def test_ok_with_logs_and_lowercase(self):
        lines = ["boot ok", "SHOT 2 2 RGB565BE", "f80007e0",
                 "001fffff", "END"]
        w, h, rgb = screenshot.parse_shot(lines)
        self.assertEqual((w, h), (2, 2))
        self.assertEqual(rgb, bytes([255, 0, 0, 0, 255, 0, 0, 0, 255, 255, 255, 255]))

    def test_wrong_length(self):
        with self.assertRaisesRegex(sl.FrameError, "row 0"):
            screenshot.parse_shot(["SHOT 2 1 RGB565BE", "F800", "END"])

    def test_non_hex_and_extra_row(self):
        with self.assertRaises(sl.FrameError):
            screenshot.parse_shot(["SHOT 1 1 RGB565BE", "ZZZZ", "END"])
        with self.assertRaisesRegex(sl.FrameError, "row 1"):
            screenshot.parse_shot(["SHOT 1 1 RGB565BE", "F800", "F800", "END"])

    def test_premature_end(self):
        with self.assertRaisesRegex(sl.FrameError, "premature"):
            screenshot.parse_shot(["SHOT 1 2 RGB565BE", "F800", "END"])

    def test_bad_dimensions(self):
        with self.assertRaises(sl.FrameError):
            screenshot.parse_shot(["SHOT 0 5 RGB565BE", "END"])
        with self.assertRaises(sl.FrameError):
            screenshot.parse_shot(["SHOT 1025 5 RGB565BE", "END"])

    def test_missing_end_and_header(self):
        with self.assertRaises(sl.LinkTimeout):
            screenshot.parse_shot(["SHOT 1 1 RGB565BE", "F800"])
        with self.assertRaises(sl.LinkTimeout):
            screenshot.parse_shot(["hello"])

    def test_err_reply(self):
        with self.assertRaisesRegex(sl.DeviceError, "no display"):
            screenshot.parse_shot(["log", "ERR no display"])


class ExportParseTests(unittest.TestCase):
    def test_ok_verbatim(self):
        night, rows = export_csv.parse_export(["x", "BEGIN night=0", "a,b", " c, d ", "", "END"])
        self.assertEqual(night, 0)
        self.assertEqual(rows, ["a,b", " c, d ", ""])

    def test_night_id(self):
        self.assertEqual(export_csv.parse_export(["BEGIN night=42", "END"]), (42, []))

    def test_err_and_missing_end(self):
        with self.assertRaisesRegex(sl.DeviceError, "no such night"):
            export_csv.parse_export(["ERR no such night"])
        with self.assertRaises(sl.LinkTimeout):
            export_csv.parse_export(["BEGIN night=1", "a"])


class IterLinesTests(unittest.TestCase):
    def test_split_chunks_and_crlf(self):
        ser = FakeSerial([b"he", b"llo\r", b"\nwor", b"ld\r\n\xffx\n"])
        t = [0]

        def clock():
            t[0] += 1
            return t[0]

        it = sl.iter_lines(ser, 100, clock)
        self.assertEqual([next(it) for _ in range(3)], ["hello", "world", "�x"])

    def test_timeout(self):
        ser = FakeSerial([b"partial"])
        t = [0]

        def clock():
            t[0] += 1
            return t[0]

        with self.assertRaises(sl.LinkTimeout):
            list(sl.iter_lines(ser, 10, clock))


if __name__ == "__main__":
    unittest.main()
