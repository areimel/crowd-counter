"""Board table lookup and the compiler defines derived from it."""
import subprocess

from config.core import ConfigError, load_yaml


def load_boards(path):
    return load_yaml(path)


def select_board(boards, board_id):
    if board_id not in boards:
        raise ConfigError("board", f"unknown board {board_id!r}; known: {sorted(boards)}")
    return boards[board_id]


def firmware_version(cwd):
    try:
        out = subprocess.run(["git", "describe", "--tags", "--always"], cwd=cwd, capture_output=True,
                             text=True, timeout=10, check=True).stdout.strip()
    except (OSError, subprocess.SubprocessError):
        out = ""
    return out or "0.0.0-dev"


def board_defines(board, version):
    """Return (name, value) string defines for CPPDEFINES; the caller quotes them."""
    return [("BOARD_LABEL", board["label"]), ("FIRMWARE_VERSION", version)]
