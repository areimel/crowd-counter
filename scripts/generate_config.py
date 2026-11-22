"""Validate config.yaml and compile it into the firmware's generated header.

Sections live in scripts/config/sections, so firmware never parses key names,
text, or labels.
"""

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from config.boards import load_boards, select_board  # noqa: E402
from config.core import (  # noqa: E402
    ConfigError, Context, fields, load_sections, load_yaml, write_if_changed,
)
import yaml  # noqa: E402

HEADER = "// Generated from config.yaml. Edit YAML, not this file."


def render(data, ctx):
    """Return the header text for `data`, raising ConfigError on the first invalid key."""
    modules = load_sections()
    fields(data, [], "", [m.KEY for m in modules])
    validated = []
    for module in modules:
        if module.KEY not in data and module.REQUIRED:
            raise ConfigError(module.KEY, "missing required section")
        validated.append((module, module.validate(data.get(module.KEY, module.DEFAULT), ctx)))
    includes = sorted({inc for m, _ in validated for inc in m.INCLUDES})
    lines = [HEADER, "#pragma once", *[f"#include {inc}" for inc in includes], "",
             "namespace AppConfig {"]
    for module, cfg in validated:
        lines += module.emit(cfg, ctx)
    lines += ["} // namespace AppConfig", ""]
    return "\n".join(lines)


def generate(source, destination, ctx):
    """Render `source` into `destination`."""
    source = Path(source)
    try:
        header = render(load_yaml(source), ctx)
    except (ConfigError, yaml.YAMLError) as exc:
        raise ConfigError("", f"Invalid {source}: {exc}") from exc
    write_if_changed(destination, header)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", default="config.yaml")
    parser.add_argument("--board", required=True)
    parser.add_argument("--output", default="build/generated/generated_config.h")
    args = parser.parse_args()
    try:
        ctx = Context(args.board, select_board(load_boards(Path("boards/boards.yaml")), args.board))
        generate(args.config, args.output, ctx)
    except (OSError, ConfigError) as exc:
        parser.exit(1, f"Configuration error: {exc}\n")
    for warning in ctx.warnings:
        print(f"warning: {warning}")
