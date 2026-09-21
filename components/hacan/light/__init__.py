import esphome.codegen as cg
from esphome.components import light, output
import esphome.config_validation as cv
from esphome.const import CONF_OUTPUT, CONF_OUTPUT_ID

from .. import EVENT_SOURCE_LIST, HacanComponent, hacan_ns

DEPENDENCIES = ["hacan"]

CONF_HACAN_ID = "hacan_id"
CONF_ENTITY_ID = "entity_id"
CONF_ENDPOINT = "endpoint"
CONF_EVENT_SOURCES = "event_sources"

HacanLightOutput = hacan_ns.class_("HacanLightOutput", light.LightOutput, cg.Component)

CONFIG_SCHEMA = light.BINARY_LIGHT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(HacanLightOutput),
        cv.GenerateID(CONF_HACAN_ID): cv.use_id(HacanComponent),
        cv.Required(CONF_ENTITY_ID): cv.hex_uint32_t,
        cv.Required(CONF_ENDPOINT): cv.int_range(min=1, max=0xFE),
        cv.Required(CONF_OUTPUT): cv.use_id(output.BinaryOutput),
        cv.Optional(CONF_EVENT_SOURCES, default=[]): EVENT_SOURCE_LIST,
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await light.register_light(var, config)
    hacan = await cg.get_variable(config[CONF_HACAN_ID])
    output_ = await cg.get_variable(config[CONF_OUTPUT])
    cg.add(var.set_hacan(hacan))
    cg.add(var.set_output(output_))
    cg.add(var.set_endpoint(config[CONF_ENDPOINT]))
    cg.add(hacan.add_owned_endpoint(config[CONF_ENTITY_ID], config[CONF_ENDPOINT], var))
    for source in config[CONF_EVENT_SOURCES]:
        listener = var.add_event_source(
            source["entity_id"], source["event"], source["action"]
        )
        cg.add(hacan.add_event_listener(listener))
