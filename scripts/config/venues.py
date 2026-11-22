"""counter.venues: venue types with starting devices-per-person ratios."""
from config.core import ConfigError, cfloat, cstr, fields, is_printable, number

MAX_VENUES, MAX_NAME = 8, 14
RATIO_LOW, RATIO_HIGH = 0.3, 5.0


def validate(value, path):
    if not isinstance(value, list) or not 1 <= len(value) <= MAX_VENUES:
        raise ConfigError(path, f"must be a list of 1-{MAX_VENUES} venues")
    venues, seen = [], set()
    for i, item in enumerate(value):
        where = f"{path}[{i}]"
        item = fields(item, ["name", "ratio_ble", "ratio_wifi"], where)
        name = item["name"]
        if (not isinstance(name, str) or not name.strip() or len(name) > MAX_NAME
                or not is_printable(name)):
            raise ConfigError(f"{where}.name", f"must be 1-{MAX_NAME} printable ASCII characters, not blank")
        if name in seen:
            raise ConfigError(f"{where}.name", f"duplicate venue name {name!r}")
        seen.add(name)
        venues.append({"name": name,
                       "ratio_ble": number(item["ratio_ble"], RATIO_LOW, RATIO_HIGH, f"{where}.ratio_ble"),
                       "ratio_wifi": number(item["ratio_wifi"], RATIO_LOW, RATIO_HIGH, f"{where}.ratio_wifi")})
    return venues


def emit(venues):
    lines = ["struct Venue { const char* name; float ratioBle; float ratioWifi; };",
             "constexpr Venue VENUES[] = {"]
    lines += [f"    {{{cstr(v['name'])}, {cfloat(v['ratio_ble'])}, {cfloat(v['ratio_wifi'])}}},"
              for v in venues]
    return lines + ["};", f"constexpr uint8_t VENUE_COUNT = {len(venues)};"]
