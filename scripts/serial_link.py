"""Shared serial helpers for the Crowd Counter host tools.

Frame parsers take any iterable of text lines, so tests can feed plain lists.
pyserial is imported lazily inside open_port(); run the tools with the repo
venv's python (.venv/bin/python) if it is missing from the system python.

open_port() creates the Serial object unopened, sets dtr/rts low, then opens it,
because pyserial otherwise asserts DTR/RTS on open, which commonly resets an
ESP32-S3 on its native USB-Serial/JTAG port and would lose the command sent right
after. This is unverified on hardware.
"""
import argparse
import sys
import time

DEFAULT_BAUD = 115200


class LinkError(Exception):
    """Base class for host-tool failures reported as a short message."""


class LinkTimeout(LinkError):
    pass


class FrameError(LinkError):
    pass


class DeviceError(LinkError):
    """The device answered `ERR <message>`."""


def open_port(port, baud=DEFAULT_BAUD, timeout=0.2):
    try:
        import serial
    except ImportError:
        raise LinkError("pyserial is not installed for this python; run the tool with the "
                        "repo venv's python, e.g. .venv/bin/python scripts/<tool>.py") from None
    try:
        ser = serial.Serial(None, baud, timeout=timeout)  # unopened, so DTR/RTS can be set first
        ser.dtr = False
        ser.rts = False
        ser.port = port
        ser.open()
    except (serial.SerialException, OSError) as exc:
        raise LinkError(f"cannot open {port}: {exc}") from None
    time.sleep(0.1)
    ser.reset_input_buffer()  # drop stale bytes from before we connected
    return ser


def send_line(ser, text):
    ser.write(text.encode("ascii") + b"\n")
    ser.flush()


def iter_lines(ser, deadline, clock=time.monotonic):
    """Yield decoded lines until `deadline` (a clock() value); then raise LinkTimeout."""
    buf = bytearray()
    while True:
        if clock() >= deadline:
            raise LinkTimeout("timed out waiting for the device")
        data = ser.read(getattr(ser, "in_waiting", 0) or 1)
        if not data:
            continue
        buf.extend(data)
        while True:
            nl = buf.find(b"\n")
            if nl < 0:
                break
            raw = bytes(buf[:nl])
            del buf[:nl + 1]
            yield raw.rstrip(b"\r").decode("utf-8", errors="replace")


def read_frame(lines, begin, end_line="END", check=None):
    """Skip lines until begin(line) is truthy, then collect lines until end_line.

    `ERR <msg>` before the start raises DeviceError. check(index, line), if given,
    is called for each body line as it arrives and may raise FrameError.
    Returns (begin_result, body_lines). A stream that ends early is a LinkTimeout.
    """
    it = iter(lines)
    head = None
    for line in it:
        if line.startswith("ERR"):
            raise DeviceError(line[3:].strip() or "device error")
        head = begin(line)
        if head:
            break
    else:
        raise LinkTimeout("stream ended before the frame header")
    body = []
    for line in it:
        if line == end_line:
            return head, body
        if check:
            check(len(body), line)
        body.append(line)
    raise LinkTimeout("stream ended before END")


def add_common_args(parser, default_timeout, default_out=None):
    parser.add_argument("--port", required=True, help="serial device, e.g. /dev/ttyACM0")
    parser.add_argument("--timeout", type=float, default=default_timeout,
                        help="seconds to wait for the whole reply (default %(default)s)")
    parser.add_argument("--baud", type=int, default=DEFAULT_BAUD)
    parser.add_argument("--out", default=default_out, help="output file")
    return parser


def request(args, command, parse, clock=time.monotonic):
    """Open the port, send `command`, return parse(lines) for the reply stream."""
    ser = open_port(args.port, args.baud)
    try:
        send_line(ser, command)
        return parse(iter_lines(ser, clock() + args.timeout, clock))
    finally:
        ser.close()


def run(main_fn, argv):
    try:
        return main_fn(argv)
    except LinkError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
