"""display: screen brightness and the idle dimming behaviour."""
from config.core import ConfigError, fields, integer

KEY, REQUIRED = "display", False
DEFAULT = {"brightness": 96, "dim_brightness": 12, "dim_after_s": 30}
INCLUDES = ["<stdint.h>"]


def validate(value, ctx):
    value = {**DEFAULT, **fields(value, [], KEY, DEFAULT)}
    brightness = integer(value["brightness"], 1, 255, "display.brightness")
    dim = integer(value["dim_brightness"], 1, 255, "display.dim_brightness")
    if dim >= brightness:
        raise ConfigError("display.dim_brightness", "must be lower than display.brightness")
    return {"brightness": brightness, "dim_brightness": dim,
            "dim_after_s": integer(value["dim_after_s"], 5, 600, "display.dim_after_s")}


def emit(cfg, ctx):
    return [f"constexpr uint8_t DISPLAY_BRIGHTNESS = {cfg['brightness']};",
            f"constexpr uint8_t DIM_BRIGHTNESS = {cfg['dim_brightness']};",
            f"constexpr uint16_t DIM_AFTER_S = {cfg['dim_after_s']};"]
