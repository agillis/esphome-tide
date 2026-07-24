import os
import sys

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import time
from esphome.const import CONF_ID, CONF_TIME_ID

CODEOWNERS = ["@agillis"]
DEPENDENCIES = ["time"]
MULTI_CONF = True

tide_ns = cg.esphome_ns.namespace("tide")
TideComponent = tide_ns.class_("TideComponent", cg.PollingComponent)

CONF_TIDE_ID = "tide_id"
CONF_STATION_DATA = "station_data"
CONF_STATION_ID = "station_id"
CONF_UNITS = "units"
CONF_PREDICTION_WINDOW_HOURS = "prediction_window_hours"

# Maps the user-facing unit to the C++ "feet?" boolean.
UNITS = {"ft": True, "m": False}

# generator/ sits next to components/ in the esphome-tide checkout.
_GENERATOR_DIR = os.path.abspath(
    os.path.join(os.path.dirname(__file__), "..", "..", "generator")
)


def _station_string_from_id(station_id: str) -> str:
    """Resolve a NOAA station id to a ``TIDE1|...`` string at build time.

    Fetches the station's harmonic constants from the openwatersio tide-database
    (reusing generator/generate_station.py) and caches the result under
    generator/.station_cache/ so subsequent builds work offline. Raises
    cv.Invalid with a clear message on any failure, so the build stops instead of
    shipping an empty/bad station.
    """
    station_id = str(station_id).strip()
    cache_dir = os.path.join(_GENERATOR_DIR, ".station_cache")
    cache_file = os.path.join(cache_dir, f"{station_id}.txt")
    if os.path.isfile(cache_file):
        with open(cache_file, "r", encoding="utf-8") as fh:
            cached = fh.read().strip()
        if cached:
            return cached

    if not os.path.isdir(_GENERATOR_DIR):
        raise cv.Invalid(
            f"tide: can't resolve station_id {station_id!r}: generator dir not "
            f"found at {_GENERATOR_DIR}"
        )
    if _GENERATOR_DIR not in sys.path:
        sys.path.insert(0, _GENERATOR_DIR)
    try:
        from generate_station import build_string, load_station
    except Exception as err:  # noqa: BLE001
        raise cv.Invalid(f"tide: could not import the station generator: {err}")
    try:
        station = load_station(station_id)  # network fetch from GitHub (first build only)
        data, _meta = build_string(station)
    except Exception as err:  # noqa: BLE001
        raise cv.Invalid(
            f"tide: failed to fetch/build harmonics for station id "
            f"{station_id!r}: {err}. Check the id at "
            f"https://tidesandcurrents.noaa.gov/map/ (it must be a harmonic "
            f"station present in the openwatersio/tide-database)."
        )

    os.makedirs(cache_dir, exist_ok=True)
    with open(cache_file, "w", encoding="utf-8") as fh:
        fh.write(data + "\n")
    return data


def _resolve_station(config):
    """Require exactly one of station_data / station_id; resolve id -> data.

    station_id lets you name a NOAA station and have the harmonic constants
    looked up at *build time* instead of pasting the long TIDE1|... string.
    """
    data = config.get(CONF_STATION_DATA, "")
    sid = config.get(CONF_STATION_ID, "")
    if bool(data) == bool(sid):
        raise cv.Invalid(
            "tide: specify exactly one of 'station_data' (the TIDE1|… string) or "
            "'station_id' (a NOAA station id; harmonics are fetched at build time)"
        )
    if sid:
        config[CONF_STATION_DATA] = _station_string_from_id(sid)
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(TideComponent),
            cv.GenerateID(CONF_TIME_ID): cv.use_id(time.RealTimeClock),
            cv.Optional(CONF_STATION_DATA, default=""): cv.string,
            cv.Optional(CONF_STATION_ID, default=""): cv.string,
            cv.Optional(CONF_UNITS, default="ft"): cv.enum(UNITS, lower=True),
            cv.Optional(CONF_PREDICTION_WINDOW_HOURS, default=30.0): cv.float_range(
                min=26.0, max=48.0
            ),
        }
    ).extend(cv.polling_component_schema("60s")),
    _resolve_station,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    time_ = await cg.get_variable(config[CONF_TIME_ID])
    cg.add(var.set_time(time_))
    cg.add(var.set_station_data(config[CONF_STATION_DATA]))
    cg.add(var.set_units_feet(config[CONF_UNITS]))
    cg.add(var.set_prediction_window_hours(config[CONF_PREDICTION_WINDOW_HOURS]))
