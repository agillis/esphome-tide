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
CONF_UNITS = "units"
CONF_PREDICTION_WINDOW_HOURS = "prediction_window_hours"

# Maps the user-facing unit to the C++ "feet?" boolean.
UNITS = {"ft": True, "m": False}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(TideComponent),
        cv.GenerateID(CONF_TIME_ID): cv.use_id(time.RealTimeClock),
        cv.Required(CONF_STATION_DATA): cv.string,
        cv.Optional(CONF_UNITS, default="ft"): cv.enum(UNITS, lower=True),
        cv.Optional(CONF_PREDICTION_WINDOW_HOURS, default=30.0): cv.float_range(
            min=26.0, max=48.0
        ),
    }
).extend(cv.polling_component_schema("60s"))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    time_ = await cg.get_variable(config[CONF_TIME_ID])
    cg.add(var.set_time(time_))
    cg.add(var.set_station_data(config[CONF_STATION_DATA]))
    cg.add(var.set_units_feet(config[CONF_UNITS]))
    cg.add(var.set_prediction_window_hours(config[CONF_PREDICTION_WINDOW_HOURS]))
