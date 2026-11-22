"""PlatformIO pre-script: validate config.yaml in PlatformIO's own Python process
and expose the board label, version and generated header to the build."""
import sys
from pathlib import Path

Import("env")  # noqa: F821 -- provided by PlatformIO/SCons

if env["PIOPLATFORM"] != "native":  # noqa: F821
    project_dir = Path(env.subst("$PROJECT_DIR"))  # noqa: F821
    sys.path.insert(0, str(project_dir / "scripts"))
    from config.boards import board_defines, firmware_version, load_boards, select_board
    from config.core import ConfigError, Context
    from generate_config import generate

    board_id = env["PIOENV"]  # noqa: F821
    config_path = project_dir / env.GetProjectOption("custom_config", "config.yaml")  # noqa: F821
    out_dir = Path(env.subst("$BUILD_DIR")) / "generated"  # noqa: F821
    try:
        ctx = Context(board_id, select_board(load_boards(project_dir / "boards/boards.yaml"), board_id))
        generate(config_path, out_dir / "generated_config.h", ctx)
    except (OSError, ConfigError) as exc:
        raise SystemExit(f"Configuration error: {exc}") from exc
    for warning in ctx.warnings:
        print(f"warning: {warning}")
    strings = board_defines(ctx.board, firmware_version(project_dir))
    env.Append(  # noqa: F821
        CPPPATH=[str(out_dir)],
        CPPDEFINES=[(k, env.StringifyMacro(v)) for k, v in strings],  # noqa: F821
    )
