import os
import tempfile
import unittest
from pathlib import Path

import yaml

from support import make_ctx, default_config
from config.core import ConfigError, UniqueKeyLoader, cfloat, cstr, fields, integer, number, write_if_changed
from generate_config import render


class CoreTests(unittest.TestCase):
    def test_duplicate_keys_rejected(self):
        with self.assertRaises(ConfigError):
            yaml.load("a: 1\na: 2\n", Loader=UniqueKeyLoader)

    def test_non_string_keys_rejected(self):
        with self.assertRaises(ConfigError):
            yaml.load("1: a\n", Loader=UniqueKeyLoader)

    def test_fields_reports_unknown_with_path(self):
        with self.assertRaises(ConfigError) as caught:
            fields({"nmae": "x"}, ["name"], "counter", optional=())
        self.assertIn("counter.nmae", str(caught.exception))

    def test_integer_bounds(self):
        self.assertEqual(integer(5, 5, 100, "x"), 5)
        for bad in (4, 101, True, "5"):
            with self.assertRaises(ConfigError):
                integer(bad, 5, 100, "x")

    def test_cstr_escapes(self):
        self.assertEqual(cstr('a"b'), '"a\\"b"')
        self.assertEqual(cstr("??="), '"\\?\\?="')

    def test_write_if_changed_keeps_mtime(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "sub" / "out.h"
            self.assertTrue(write_if_changed(path, "x"))
            os.utime(path, (1000, 1000))
            self.assertFalse(write_if_changed(path, "x"))
            self.assertEqual(path.stat().st_mtime, 1000)
            self.assertTrue(write_if_changed(path, "y"))

    def test_unknown_top_level_key(self):
        data = default_config()
        data["bluetoth"] = {}
        with self.assertRaises(ConfigError) as caught:
            render(data, make_ctx())
        self.assertIn("bluetoth", str(caught.exception))

    def test_missing_required_section(self):
        for key in ("project", "counter"):
            data = default_config()
            del data[key]
            with self.assertRaises(ConfigError) as caught:
                render(data, make_ctx())
            self.assertIn(key, str(caught.exception))

    def test_number_bounds(self):
        self.assertEqual(number(1, 0.3, 5.0, "x"), 1)
        self.assertEqual(number(0.3, 0.3, 5.0, "x"), 0.3)
        self.assertEqual(number(5.0, 0.3, 5.0, "x"), 5.0)
        for bad in (0.29, 5.01, True, "1.0", None):
            with self.assertRaises(ConfigError):
                number(bad, 0.3, 5.0, "x")

    def test_cfloat_literals(self):
        self.assertEqual(cfloat(1.45), "1.45f")
        self.assertEqual(cfloat(1), "1.0f")
        self.assertEqual(cfloat(0.8), "0.8f")
        self.assertEqual(cfloat(0), "0.0f")


if __name__ == "__main__":
    unittest.main()
