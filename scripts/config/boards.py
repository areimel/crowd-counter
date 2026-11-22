"""Board table lookup and the compiler defines derived from it."""
import subprocess

from config.core import ConfigError, load_yaml


def load_boards(path):
    return load_yaml(path)


def select_board(boards, board_id):
    if board_id not in boards:
        raise ConfigError("board", f"unknown board {board_id!r}; known: {sorted(boards)}")
    return boards[board_id]


def button_count(board):
    return board["buttons"]


def firmware_version(cwd):
    try:
        out = subprocess.run(["git", "describe", "--tags", "--always"], cwd=cwd, capture_output=True,
                             text=True, timeout=10, check=True).stdout.strip()
    except (OSError, subprocess.SubprocessError):
        out = ""
    return out or "0.0.0-dev"


def board_defines(board_id, board, version):
    """Return (strings, plain) lists of (name, value) for CPPDEFINES; strings are quoted by the caller."""
    # M5Unified reads a numeric BOARD_ID macro, so ours is APP_BOARD_ID.
    strings = [("APP_BOARD_ID", board_id), ("BOARD_LABEL", board["label"]), ("FIRMWARE_VERSION", version)]
    plain = [("BOARD_DRIVER_M5", 1), ("BOARD_BUTTON_COUNT", button_count(board)),
             ("BOARD_HAS_DISPLAY", int(board.get("display", False))),
             ("BOARD_HAS_BATTERY", int(board.get("battery", False)))]
    return strings, plain
