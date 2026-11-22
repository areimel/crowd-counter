"""Built-in config sections. Each module exposes KEY, REQUIRED, DEFAULT, INCLUDES,
validate(value, ctx) and emit(cfg, ctx)."""
from . import project, display, gestures, counter

BUILTIN = [project, display, gestures, counter]
