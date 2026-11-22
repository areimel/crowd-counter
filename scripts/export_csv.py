"""Download one night's CSV from the device over the serial CLI.

Usage: .venv/bin/python scripts/export_csv.py --port /dev/ttyACM0 [N | --night N] [--out f.csv]

Wire format (host sends `export\\n` or `export N\\n`). Log lines before BEGIN are
skipped, `ERR <message>` before BEGIN aborts:

    BEGIN night=<id>
    <CSV lines, taken verbatim>
    END

A CSV line that is exactly `END` cannot occur because the first CSV field is
never `END`. Output defaults to night-<id>.csv.
"""
import argparse
import re
import sys

from serial_link import add_common_args, read_frame, request, run

BEGIN = re.compile(r"^BEGIN night=(\d+)$")


def parse_export(lines):
    """Return (night_id, csv_lines)."""
    def begin(line):
        m = BEGIN.match(line)
        return m and int(m.group(1)) + 1  # +1 keeps night 0 truthy
    head, body = read_frame(lines, begin, "END")
    return head - 1, body


def main(argv=None):
    ap = add_common_args(argparse.ArgumentParser(description=__doc__.splitlines()[0]), 30)
    ap.add_argument("night_pos", nargs="?", type=int, metavar="N", help="night id")
    ap.add_argument("--night", type=int, help="night id (same as positional N)")
    args = ap.parse_args(argv)
    night = args.night if args.night is not None else args.night_pos
    night_id, rows = request(args, "export" if night is None else f"export {night}", parse_export)
    path = args.out or f"night-{night_id}.csv"
    with open(path, "w", encoding="utf-8", newline="") as f:
        f.write("\n".join(rows) + "\n" if rows else "")
    print(f"{path} ({len(rows)} lines, night {night_id})")
    return 0


if __name__ == "__main__":
    sys.exit(run(main, None))
