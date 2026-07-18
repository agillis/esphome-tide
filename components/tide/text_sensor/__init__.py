import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_TYPE

from .. import CONF_TIDE_ID, TideComponent, tide_ns

DEPENDENCIES = ["tide"]

TideTextSensor = tide_ns.class_(
    "TideTextSensor", text_sensor.TextSensor, cg.PollingComponent
)
TextType = tide_ns.enum("TideTextSensorType")

TYPES = {
    "high_time": TextType.TIDE_TEXT_HIGH_TIME,
    "low_time": TextType.TIDE_TEXT_LOW_TIME,
}

CONF_FORMAT = "format"

CONFIG_SCHEMA = (
    text_sensor.text_sensor_schema(TideTextSensor)
    .extend(
        {
            cv.GenerateID(CONF_TIDE_ID): cv.use_id(TideComponent),
            cv.Required(CONF_TYPE): cv.enum(TYPES, lower=True),
            # strftime format; default reproduces the NOAA module's "10:58 AM".
            cv.Optional(CONF_FORMAT, default="%I:%M %p"): cv.string,
        }
    )
    .extend(cv.polling_component_schema("60s"))
)


async def to_code(config):
    var = await text_sensor.new_text_sensor(config)
    await cg.register_component(var, config)
    cg.add(var.set_type(config[CONF_TYPE]))
    cg.add(var.set_format(config[CONF_FORMAT]))
    parent = await cg.get_variable(config[CONF_TIDE_ID])
    cg.add(var.set_parent(parent))
