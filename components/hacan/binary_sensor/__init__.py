import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID

from .. import HacanComponent, hacan_ns

DEPENDENCIES = ["hacan"]

CONF_HACAN_ID = "hacan_id"
CONF_SOURCE_ID = "source_id"
CONF_ENTITY_ID = "entity_id"
CONF_ENDPOINT = "endpoint"

HacanBinarySensor = hacan_ns.class_(
    "HacanBinarySensor", binary_sensor.BinarySensor, cg.Component
)

CONFIG_SCHEMA = (
    binary_sensor.binary_sensor_schema(HacanBinarySensor)
    .extend(
        {
            cv.GenerateID(CONF_HACAN_ID): cv.use_id(HacanComponent),
            cv.Required(CONF_SOURCE_ID): cv.use_id(binary_sensor.BinarySensor),
            cv.Required(CONF_ENTITY_ID): cv.hex_uint32_t,
            cv.Required(CONF_ENDPOINT): cv.int_range(min=1, max=0xFE),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await binary_sensor.new_binary_sensor(config)
    await cg.register_component(var, config)
    hacan = await cg.get_variable(config[CONF_HACAN_ID])
    source = await cg.get_variable(config[CONF_SOURCE_ID])
    cg.add(var.set_hacan(hacan))
    cg.add(var.set_source(source))
    cg.add(var.set_endpoint(config[CONF_ENDPOINT]))
    cg.add(hacan.add_owned_endpoint(config[CONF_ENTITY_ID], config[CONF_ENDPOINT], cg.nullptr))
