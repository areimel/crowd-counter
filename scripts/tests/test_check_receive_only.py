import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import check_receive_only as cro  # noqa: E402


class ScanTextTests(unittest.TestCase):
    def test_every_forbidden_api_is_caught(self):
        samples = ["NimBLEAdvertising* a;", "NimBLEClient* c;", "NimBLEServer* s;",
                   "scan->setActiveScan(true);", "scan->setActiveScan( true );",
                   "esp_wifi_scan_start(0, true);", "esp_wifi_connect();",
                   "esp_wifi_80211_tx(0, b, 1, false);", "esp_now_init();",
                   "ble_gap_connect(0, 0, 0, 0, 0, 0);", "ble_gap_adv_start(0);",
                   "WiFi.begin(s, p);", "WiFiClient c;", "HTTPClient h;"]
        for sample in samples:
            with self.subTest(sample=sample):
                self.assertEqual(len(cro.scan_text(sample)), 1)

    def test_allowed_calls_pass(self):
        ok = "scan->setActiveScan(false);\nesp_wifi_set_promiscuous(true);\n#include <WiFi.h>\n"
        self.assertEqual(cro.scan_text(ok), [])

    def test_comments_and_strings_ignored(self):
        text = ('// esp_now_init();\n/* WiFi.begin();\n   HTTPClient h; */\n'
                'const char* s = "esp_wifi_connect";\nint x;  // setActiveScan(true)\n')
        self.assertEqual(cro.scan_text(text), [])

    def test_line_numbers_survive_block_comments(self):
        text = "/* a\nb\nc */\nesp_now_send();\n"
        self.assertEqual(cro.scan_text(text), [(4, "esp_now_")])

    def test_code_after_comment_still_scanned(self):
        self.assertEqual(len(cro.scan_text("/* ok */ esp_wifi_connect();")), 1)


class ScanTreeTests(unittest.TestCase):
    def test_reports_file_and_line_and_exit_code(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "sub").mkdir()
            (root / "sub" / "bad.cpp").write_text("int a;\nWiFi.begin();\n", encoding="utf-8")
            (root / "good.h").write_text("// WiFi.begin();\n", encoding="utf-8")
            (root / "notes.txt").write_text("WiFi.begin();\n", encoding="utf-8")
            hits = cro.scan_tree([root])
            self.assertEqual(hits, [(root / "sub" / "bad.cpp", 2, "WiFi.")])
            self.assertEqual(cro.main([str(root)]), 1)

    def test_clean_tree_passes(self):
        with tempfile.TemporaryDirectory() as tmp:
            Path(tmp, "ok.cpp").write_text("int main() { return 0; }\n", encoding="utf-8")
            self.assertEqual(cro.main([tmp]), 0)

    def test_missing_root_is_an_error(self):
        self.assertEqual(cro.main(["/nonexistent/receive-only-root"]), 2)


if __name__ == "__main__":
    unittest.main()
