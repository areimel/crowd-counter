"""counter: estimator, radio, venue and alert settings. Every key is required:
config.yaml is the single source of these values."""
from config import venues
from config.core import ConfigError, boolean, cfloat, fields, integer

KEY, REQUIRED, DEFAULT = "counter", True, None
INCLUDES = ["<stdint.h>"]

# (key, low, high) for plain integers.
SCALARS = [("window_s", 60, 900), ("rssi_gate_dbm", -95, -50), ("recompute_s", 1, 30),
           ("smoothing_s", 1, 300), ("peak_hold_min", 1, 30), ("alert_hold_s", 0, 300),
           ("table_capacity", 500, 6000), ("background_after_min", 10, 600),
           ("blend_ble_pct", 0, 100)]
# Order matches cc::DeviceClass: PHONE, WEARABLE, AUDIO, TRACKER, COMPUTER, BEACON, OTHER.
CLASSES = ["phone", "wearable", "audio", "tracker", "computer", "beacon", "other"]
MAX_CHANNELS = 3


def _threshold(value):
    path = f"{KEY}.threshold"
    value = fields(value, ["default", "step", "max"], path)
    step = integer(value["step"], 1, 50, f"{path}.step")
    top = integer(value["max"], step, 9999, f"{path}.max")
    default = integer(value["default"], step, top, f"{path}.default")
    if default % step:
        raise ConfigError(f"{path}.default", f"must be a multiple of step ({step})")
    return {"default": default, "step": step, "max": top}


def _radio(value):
    path = f"{KEY}.radio"
    value = fields(value, ["ble_slice_ms", "wifi_dwell_ms", "channels"], path)
    channels = value["channels"]
    if not isinstance(channels, list) or not 1 <= len(channels) <= MAX_CHANNELS:
        raise ConfigError(f"{path}.channels", f"must be a list of 1-{MAX_CHANNELS} channels")
    channels = [integer(c, 1, 13, f"{path}.channels[{i}]") for i, c in enumerate(channels)]
    if len(set(channels)) != len(channels):
        raise ConfigError(f"{path}.channels", "channels must be distinct")
    return {"ble_slice_ms": integer(value["ble_slice_ms"], 200, 10000, f"{path}.ble_slice_ms"),
            "wifi_dwell_ms": integer(value["wifi_dwell_ms"], 200, 10000, f"{path}.wifi_dwell_ms"),
            "channels": channels}


def _alert(value):
    path = f"{KEY}.alert"
    value = fields(value, ["flash", "chime", "volume"], path)
    return {"flash": boolean(value["flash"], f"{path}.flash"),
            "chime": boolean(value["chime"], f"{path}.chime"),
            "volume": integer(value["volume"], 0, 160, f"{path}.volume")}


def validate(value, ctx):
    value = fields(value, [k for k, _, _ in SCALARS] + ["threshold", "class_weights_pct", "radio",
                                                         "venues", "alert"], KEY)
    cfg = {k: integer(value[k], low, high, f"{KEY}.{k}") for k, low, high in SCALARS}
    recompute = cfg["recompute_s"]
    smoothing = integer(value["smoothing_s"], recompute, 300, f"{KEY}.smoothing_s")
    if smoothing % recompute:
        raise ConfigError(f"{KEY}.smoothing_s", f"must be a multiple of recompute_s ({recompute})")
    path = f"{KEY}.class_weights_pct"
    weights = fields(value["class_weights_pct"], CLASSES, path)
    cfg["class_weights_pct"] = {c: integer(weights[c], 0, 100, f"{path}.{c}") for c in CLASSES}
    cfg.update(threshold=_threshold(value["threshold"]), radio=_radio(value["radio"]),
               venues=venues.validate(value["venues"], f"{KEY}.venues"), alert=_alert(value["alert"]))
    return cfg


def emit(cfg, ctx):
    t, r, a = cfg["threshold"], cfg["radio"], cfg["alert"]
    weights = ", ".join(cfloat(cfg["class_weights_pct"][c] / 100) for c in CLASSES)
    channels = ", ".join(map(str, r["channels"]))
    lines = [
        f"constexpr uint32_t WINDOW_S = {cfg['window_s']};",
        f"constexpr int8_t RSSI_GATE_DBM = {cfg['rssi_gate_dbm']};",
        f"constexpr uint16_t RECOMPUTE_S = {cfg['recompute_s']};",
        f"constexpr uint16_t SMOOTHING_S = {cfg['smoothing_s']};",
        f"constexpr uint16_t PEAK_HOLD_S = {cfg['peak_hold_min'] * 60};",
        f"constexpr uint16_t ALERT_HOLD_S = {cfg['alert_hold_s']};",
        f"constexpr uint32_t TABLE_CAPACITY = {cfg['table_capacity']};",
        f"constexpr uint32_t BACKGROUND_AFTER_S = {cfg['background_after_min'] * 60};",
        f"constexpr float BLEND_BLE = {cfloat(cfg['blend_ble_pct'] / 100)};",
        f"constexpr uint16_t THRESHOLD_DEFAULT = {t['default']};",
        f"constexpr uint16_t THRESHOLD_STEP = {t['step']};",
        f"constexpr uint16_t THRESHOLD_MAX = {t['max']};",
        f"constexpr float CLASS_WEIGHTS[7] = {{{weights}}};  // {', '.join(c.upper() for c in CLASSES)}",
        f"constexpr uint16_t BLE_SLICE_MS = {r['ble_slice_ms']};",
        f"constexpr uint16_t WIFI_DWELL_MS = {r['wifi_dwell_ms']};",
        f"constexpr uint8_t WIFI_CHANNELS[] = {{{channels}}};",
        f"constexpr uint8_t WIFI_CHANNEL_COUNT = {len(r['channels'])};",
    ]
    lines += venues.emit(cfg["venues"])
    lines += [f"constexpr bool ALERT_FLASH = {str(a['flash']).lower()};",
              f"constexpr bool ALERT_CHIME = {str(a['chime']).lower()};",
              f"constexpr uint8_t ALERT_VOLUME = {a['volume']};"]
    return lines
