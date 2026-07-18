import esphome.codegen as cg
from esphome.components import time as time_
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@test"]

host_time_ns = cg.esphome_ns.namespace("host_time")
HostTime = host_time_ns.class_("HostTime", time_.RealTimeClock)

CONFIG_SCHEMA = time_.TIME_SCHEMA.extend(
    {cv.GenerateID(): cv.declare_id(HostTime)}
).extend(cv.polling_component_schema("60s"))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await time_.register_time(var, config)
