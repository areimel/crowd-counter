#!/usr/bin/env python3
"""Fail if src/ or lib/ use a transmit- or connect-capable radio API.

The Crowd Counter must only passively scan BLE and receive promiscuous Wi-Fi
frames. The transmit-capable stack is linked in by the Arduino build but must
never be invoked, so this scans C/C++ sources for the APIs that would invoke it.
Comments and string literals are ignored. Exit status is 1 (listing file:line
and the API) on any hit. Stdlib only.

Usage: python scripts/check_receive_only.py [root ...]   (default: src lib)
"""
import re
import sys
from pathlib import Path

FORBIDDEN = {
    "NimBLEAdvertising": r"\bNimBLEAdvertising\b",
    "NimBLEClient": r"\bNimBLEClient\b",
    "NimBLEServer": r"\bNimBLEServer\b",
    "setActiveScan(true)": r"\bsetActiveScan\s*\(\s*true\s*\)",
    "esp_wifi_scan_start": r"\besp_wifi_scan_start\b",
    "esp_wifi_connect": r"\besp_wifi_connect\b",
    "esp_wifi_80211_tx": r"\besp_wifi_80211_tx\b",
    "esp_now_": r"\besp_now_",
    "ble_gap_connect": r"\bble_gap_connect\b",
    "ble_gap_adv_": r"\bble_gap_adv_",
    "WiFi.": r"\bWiFi\.(?!h\b)",  # the Arduino WiFi class; <WiFi.h> alone is not a call
    "WiFiClient": r"\bWiFiClient\b",
    "HTTPClient": r"\bHTTPClient\b",
}
PATTERNS = {name: re.compile(rx) for name, rx in FORBIDDEN.items()}
SUFFIXES = {".h", ".hpp", ".hh", ".c", ".cc", ".cpp", ".cxx", ".ino"}
# Comment or string/char literal, whichever starts first; comments and string
# contents are blanked (newlines kept) so line numbers stay correct.
_TOKEN = re.compile(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\'', re.S)


def strip_noise(text):
    def blank(match):
        tok = match.group(0)
        if tok[0] in "\"'":
            return tok[0] + tok[0]
        return re.sub(r"[^\n]", " ", tok)

    return _TOKEN.sub(blank, text)


def scan_text(text):
    """Return [(line_number, api_name)] for forbidden APIs in `text`."""
    hits = []
    for number, line in enumerate(strip_noise(text).splitlines(), 1):
        for name, pattern in PATTERNS.items():
            if pattern.search(line):
                hits.append((number, name))
    return hits


def scan_tree(roots):
    """Return [(path, line_number, api_name)] across all source files under `roots`."""
    found = []
    for root in roots:
        root = Path(root)
        paths = [root] if root.is_file() else sorted(root.rglob("*"))
        for path in paths:
            if path.is_file() and path.suffix.lower() in SUFFIXES:
                text = path.read_text(encoding="utf-8", errors="replace")
                found += [(path, n, name) for n, name in scan_text(text)]
    return found


def main(argv=None):
    roots = (argv if argv is not None else sys.argv[1:]) or ["src", "lib"]
    missing = [r for r in roots if not Path(r).exists()]
    if missing:
        print(f"error: no such path: {', '.join(missing)}", file=sys.stderr)
        return 2
    hits = scan_tree(roots)
    for path, number, name in hits:
        print(f"{path}:{number}: forbidden transmit/connect API {name}")
    if hits:
        print(f"{len(hits)} receive-only violation(s)", file=sys.stderr)
        return 1
    print("receive-only check passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
