import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import CONF_TYPE

from .. import CONF_TIDE_ID, TideComponent, tide_ns

DEPENDENCIES = ["tide"]

TideSensor = tide_ns.class_("TideSensor", sensor.Sensor, cg.PollingComponent)
SensorType = tide_ns.enum("TideSensorType")

TYPES = {
    "current_height": SensorType.TIDE_SENSOR_CURRENT_HEIGHT,
    "tide_percentage": SensorType.TIDE_SENSOR_PERCENTAGE,
    "high_level": SensorType.TIDE_SENSOR_HIGH_LEVEL,
    "low_level": SensorType.TIDE_SENSOR_LOW_LEVEL,
    "next_high_level": SensorType.TIDE_SENSOR_NEXT_HIGH_LEVEL,
    "next_low_level": SensorType.TIDE_SENSOR_NEXT_LOW_LEVEL,
    "mean_high_water": SensorType.TIDE_SENSOR_MEAN_HIGH_WATER,
    "mean_low_water": SensorType.TIDE_SENSOR_MEAN_LOW_WATER,
    "high_epoch": SensorType.TIDE_SENSOR_HIGH_EPOCH,
    "low_epoch": SensorType.TIDE_SENSOR_LOW_EPOCH,
    "next_high_epoch": SensorType.TIDE_SENSOR_NEXT_HIGH_EPOCH,
    "next_low_epoch": SensorType.TIDE_SENSOR_NEXT_LOW_EPOCH,
}

CONFIG_SCHEMA = (
    sensor.sensor_schema(TideSensor, accuracy_decimals=2)
    .extend(
        {
            cv.GenerateID(CONF_TIDE_ID): cv.use_id(TideComponent),
            cv.Required(CONF_TYPE): cv.enum(TYPES, lower=True),
        }
    )
    .extend(cv.polling_component_schema("60s"))
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    cg.add(var.set_type(config[CONF_TYPE]))
    parent = await cg.get_variable(config[CONF_TIDE_ID])
    cg.add(var.set_parent(parent))
