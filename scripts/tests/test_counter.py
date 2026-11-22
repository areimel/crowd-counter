import copy
import unittest

from support import make_ctx, default_config
from config.core import ConfigError
from generate_config import render

EXPECTED = [
    "constexpr uint32_t WINDOW_S = 300;",
    "constexpr int8_t RSSI_GATE_DBM = -85;",
    "constexpr int8_t RSSI_GATE_MIN = -95;",
    "constexpr int8_t RSSI_GATE_MAX = -50;",
    "constexpr uint16_t RECOMPUTE_S = 5;",
    "constexpr uint16_t SMOOTHING_S = 60;",
    "constexpr uint16_t PEAK_HOLD_S = 600;",
    "constexpr uint16_t ALERT_HOLD_S = 30;",
    "constexpr uint32_t TABLE_CAPACITY = 5000;",
    "constexpr uint32_t BACKGROUND_AFTER_S = 5400;",
    "constexpr float BLEND_BLE = 0.8f;",
    "constexpr uint16_t THRESHOLD_DEFAULT = 120;",
    "constexpr uint16_t THRESHOLD_STEP = 10;",
    "constexpr uint16_t THRESHOLD_MAX = 990;",
    "constexpr float CLASS_WEIGHTS[7] = {1.0f, 0.25f, 0.25f, 0.0f, 0.25f, 0.0f, 0.5f};"
    "  // PHONE, WEARABLE, AUDIO, TRACKER, COMPUTER, BEACON, OTHER",
    "constexpr uint16_t BLE_SLICE_MS = 2000;",
    "constexpr uint16_t WIFI_DWELL_MS = 1000;",
    "constexpr uint8_t WIFI_CHANNELS[] = {1, 6, 11};",
    "constexpr uint8_t WIFI_CHANNEL_COUNT = 3;",
    "struct Venue { const char* name; float ratioBle; float ratioWifi; };",
    "constexpr Venue VENUES[] = {",
    '    {"BAR / PUB", 1.45f, 1.0f},',
    '    {"PRIVATE EVENT", 1.3f, 1.0f},',
    "};",
    "constexpr uint8_t VENUE_COUNT = 2;",
    "constexpr bool ALERT_FLASH = true;",
    "constexpr bool ALERT_CHIME = true;",
    "constexpr uint8_t ALERT_VOLUME = 96;",
]

# key -> (low, high) for plain integer keys
BOUNDS = {"window_s": (60, 900), "rssi_gate_dbm": (-95, -50), "recompute_s": (1, 30),
          "peak_hold_min": (1, 30), "alert_hold_s": (0, 300), "table_capacity": (500, 6000),
          "background_after_min": (10, 600), "blend_ble_pct": (0, 100)}


class CounterTests(unittest.TestCase):
    def setUp(self):
        self.data = default_config()

    def set(self, path, value):
        data = copy.deepcopy(self.data)
        node = data["counter"]
        *parents, last = path.split(".")
        for part in parents:
            node = node[part]
        if value is KeyError:
            del node[last]
        else:
            node[last] = value
        return data

    def accepted(self, path, value):
        return render(self.set(path, value), make_ctx())

    def rejected(self, path, value, fragment=None):
        with self.assertRaises(ConfigError) as caught:
            render(self.set(path, value), make_ctx())
        self.assertIn(f"counter.{path}", str(caught.exception))
        if fragment:
            self.assertIn(fragment, str(caught.exception))

    def test_default_lines_verbatim(self):
        lines = render(self.data, make_ctx()).splitlines()
        start = lines.index(EXPECTED[0])
        self.assertEqual(lines[start:start + len(EXPECTED)], EXPECTED)
        self.assertIn("#include <stdint.h>", lines)

    def test_integer_bounds(self):
        for key, (low, high) in BOUNDS.items():
            with self.subTest(key=key):
                self.accepted(key, low)
                self.accepted(key, high)
                for bad in (low - 1, high + 1, "5", 5.5, True):
                    self.rejected(key, bad)

    def test_derived_values(self):
        self.assertIn("PEAK_HOLD_S = 1800;", self.accepted("peak_hold_min", 30))
        self.assertIn("BACKGROUND_AFTER_S = 600;", self.accepted("background_after_min", 10))
        self.assertIn("BLEND_BLE = 0.0f;", self.accepted("blend_ble_pct", 0))
        self.assertIn("BLEND_BLE = 1.0f;", self.accepted("blend_ble_pct", 100))

    def test_smoothing(self):
        self.accepted("smoothing_s", 5)
        self.accepted("smoothing_s", 300)
        self.rejected("smoothing_s", 4)        # below recompute_s
        self.rejected("smoothing_s", 305)
        self.rejected("smoothing_s", 62, "multiple of recompute_s")
        self.rejected("smoothing_s", "60")
        self.rejected("smoothing_s", True)

    def test_threshold(self):
        self.accepted("threshold.step", 1)
        self.assertIn("THRESHOLD_STEP = 40;", self.accepted("threshold.step", 40))
        self.rejected("threshold.step", 0)
        self.rejected("threshold.step", 51)
        with self.assertRaises(ConfigError) as caught:   # default 120 is not a multiple of 50
            render(self.set("threshold.step", 50), make_ctx())
        self.assertIn("counter.threshold.default", str(caught.exception))
        self.rejected("threshold.default", 125, "multiple")
        self.rejected("threshold.default", 0)
        self.rejected("threshold.default", 5)             # below step
        self.rejected("threshold.default", 1000)          # above max
        self.rejected("threshold.default", "120")
        self.accepted("threshold.max", 9999)
        self.rejected("threshold.max", 10000)
        self.rejected("threshold.max", "990")
        with self.assertRaises(ConfigError) as caught:   # max below default is reported on default
            render(self.set("threshold.max", 110), make_ctx())
        self.assertIn("counter.threshold.default", str(caught.exception))
        self.accepted("threshold.default", 990)

    def test_class_weights(self):
        for cls in ("phone", "wearable", "audio", "tracker", "computer", "beacon", "other"):
            self.accepted(f"class_weights_pct.{cls}", 0)
            self.accepted(f"class_weights_pct.{cls}", 100)
            for bad in (-1, 101, "5", 2.5, True):
                self.rejected(f"class_weights_pct.{cls}", bad)
        with self.assertRaises(ConfigError) as caught:
            render(self.set("class_weights_pct.other", KeyError), make_ctx())
        self.assertIn("counter.class_weights_pct: missing fields ['other']", str(caught.exception))
        self.rejected("class_weights_pct.gadget", 5, "unknown field")

    def test_radio_timing(self):
        for key in ("ble_slice_ms", "wifi_dwell_ms"):
            self.accepted(f"radio.{key}", 200)
            self.accepted(f"radio.{key}", 10000)
            for bad in (199, 10001, "1000", True):
                self.rejected(f"radio.{key}", bad)

    def test_channels(self):
        self.accepted("radio.channels", [13])
        self.accepted("radio.channels", [1, 2, 3])
        self.rejected("radio.channels", [])
        self.rejected("radio.channels", [1, 6, 11, 13])
        self.rejected("radio.channels", [1, 1], "distinct")
        for bad, idx in ((0, 1), (14, 1), ("6", 1), (True, 1)):
            with self.assertRaises(ConfigError) as caught:
                render(self.set("radio.channels", [1, bad, 11]), make_ctx())
            self.assertIn(f"counter.radio.channels[{idx}]", str(caught.exception))
        self.rejected("radio.channels", "1,6,11")

    def venue(self, **changes):
        return [{"name": "BAR", "ratio_ble": 1.0, "ratio_wifi": 1.0, **changes}]

    def test_venues(self):
        self.accepted("venues", [{"name": f"V{i}", "ratio_ble": 1, "ratio_wifi": 1} for i in range(8)])
        self.assertIn('{"A", 1.0f, 0.3f},', self.accepted("venues", self.venue(name="A", ratio_wifi=0.3)))
        self.rejected("venues", [])
        self.rejected("venues", [{"name": f"V{i}", "ratio_ble": 1, "ratio_wifi": 1} for i in range(9)])
        self.rejected("venues", "bar")

    def test_venue_names_and_ratios(self):
        def bad(venues, path):
            with self.assertRaises(ConfigError) as caught:
                render(self.set("venues", venues), make_ctx())
            self.assertIn(f"counter.{path}", str(caught.exception))

        bad([self.venue()[0], self.venue()[0]], "venues[1].name")
        bad(self.venue(name="X" * 15), "venues[0].name")
        bad(self.venue(name="CAFÉ"), "venues[0].name")
        bad(self.venue(name="   "), "venues[0].name")
        bad(self.venue(name=""), "venues[0].name")
        bad(self.venue(name=5), "venues[0].name")
        self.accepted("venues", self.venue(name="X" * 14))
        for key in ("ratio_ble", "ratio_wifi"):
            self.accepted("venues", self.venue(**{key: 0.3}))
            self.accepted("venues", self.venue(**{key: 5.0}))
            for value in (0.29, 5.01, True, "1.0", None):
                bad(self.venue(**{key: value}), f"venues[0].{key}")
        extra = self.venue()
        extra[0]["colour"] = "red"
        bad(extra, "venues[0].colour")

    def test_alert(self):
        self.accepted("alert.volume", 0)
        self.accepted("alert.volume", 160)
        self.rejected("alert.volume", 161)
        self.rejected("alert.volume", -1)
        self.rejected("alert.volume", "96")
        for key in ("flash", "chime"):
            self.assertIn(f"ALERT_{key.upper()} = false;", self.accepted(f"alert.{key}", False))
            self.rejected(f"alert.{key}", 1)
            self.rejected(f"alert.{key}", "true")

    def test_missing_keys_rejected(self):
        paths = [*BOUNDS, "smoothing_s", "threshold", "class_weights_pct", "radio", "venues", "alert",
                 "threshold.default", "threshold.step", "threshold.max", "radio.ble_slice_ms",
                 "radio.wifi_dwell_ms", "radio.channels", "alert.flash", "alert.chime", "alert.volume"]
        for path in paths:
            with self.subTest(path=path):
                with self.assertRaises(ConfigError) as caught:
                    render(self.set(path, KeyError), make_ctx())
                self.assertIn("counter", str(caught.exception))
                self.assertIn("missing", str(caught.exception))

    def test_unknown_key_rejected(self):
        self.rejected("surprise", 1, "unknown field")
        self.rejected("radio.surprise", 1, "unknown field")

    def test_counter_must_be_mapping(self):
        self.data["counter"] = []
        with self.assertRaises(ConfigError):
            render(self.data, make_ctx())


if __name__ == "__main__":
    unittest.main()
