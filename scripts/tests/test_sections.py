import copy
import unittest

from support import make_ctx, default_config
from config.core import ConfigError
from generate_config import render


class SectionTests(unittest.TestCase):
    def setUp(self):
        self.data = default_config()

    def rejected(self, section, key, value, fragment=None):
        data = copy.deepcopy(self.data)
        data[section][key] = value
        with self.assertRaises(ConfigError) as caught:
            render(data, make_ctx())
        self.assertIn(f"{section}.{key}", str(caught.exception))
        if fragment:
            self.assertIn(fragment, str(caught.exception))

    def test_default_renders(self):
        header = render(self.data, make_ctx())
        self.assertIn("#pragma once", header)
        self.assertIn("namespace AppConfig {", header)
        self.assertIn('PROJECT_NAME = "Crowd Counter"', header)
        self.assertIn("constexpr uint8_t DISPLAY_BRIGHTNESS = 96;", header)
        self.assertIn("constexpr uint8_t DIM_BRIGHTNESS = 12;", header)
        self.assertIn("constexpr uint16_t DIM_AFTER_S = 30;", header)
        self.assertIn("DEBOUNCE_MS = 25;", header)
        self.assertIn("DOUBLE_CLICK_MS = 250;", header)
        self.assertIn("LONG_PRESS_MS = 650;", header)

    def test_optional_sections_default(self):
        for key in ("display", "gestures"):
            del self.data[key]
        header = render(self.data, make_ctx())
        self.assertIn("DISPLAY_BRIGHTNESS = 96;", header)
        self.assertIn("DIM_BRIGHTNESS = 12;", header)
        self.assertIn("DIM_AFTER_S = 30;", header)
        self.assertIn("LONG_PRESS_MS = 650;", header)

    def test_partial_overrides(self):
        self.data["display"] = {"brightness": 200}
        self.data["gestures"] = {"long_press_ms": 900}
        header = render(self.data, make_ctx())
        self.assertIn("DISPLAY_BRIGHTNESS = 200;", header)
        self.assertIn("DIM_BRIGHTNESS = 12;", header)
        self.assertIn("LONG_PRESS_MS = 900;", header)
        self.assertIn("DEBOUNCE_MS = 25;", header)

    def test_display_enabled_removed(self):
        self.rejected("display", "enabled", True, "unknown field")

    def test_bounds(self):
        self.rejected("project", "name", "")
        self.rejected("project", "name", "x" * 33)
        self.rejected("display", "brightness", 0)
        self.rejected("display", "brightness", 256)
        self.rejected("display", "dim_brightness", 0)
        self.rejected("display", "dim_after_s", 4)
        self.rejected("display", "dim_after_s", 601)
        self.rejected("gestures", "debounce_ms", 0)
        self.rejected("gestures", "double_click_ms", 49)
        self.rejected("gestures", "long_press_ms", 99)

    def test_dim_brightness_must_be_below_brightness(self):
        self.rejected("display", "dim_brightness", 96, "lower than")
        self.rejected("display", "dim_brightness", 200)
        self.data["display"]["dim_brightness"] = 95
        render(self.data, make_ctx())

    def test_gesture_ordering_message_unchanged(self):
        self.data["gestures"] = {"debounce_ms": 100, "double_click_ms": 80, "long_press_ms": 650}
        with self.assertRaises(ConfigError) as caught:
            render(self.data, make_ctx())
        self.assertIn("Gesture timing must satisfy debounce_ms < double_click_ms < long_press_ms",
                      str(caught.exception))


if __name__ == "__main__":
    unittest.main()
