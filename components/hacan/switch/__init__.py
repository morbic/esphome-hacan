import esphome.codegen as cg
from esphome.components import output, switch
import esphome.config_validation as cv

from .. import EVENT_SOURCE_LIST, HacanComponent, hacan_ns

DEPENDENCIES = ["hacan"]

CONF_HACAN_ID = "hacan_id"
CONF_ENTITY_ID = "entity_id"
CONF_ENDPOINT = "endpoint"
CONF_OUTPUT = "output"
CONF_STATE_SOURCE_ENTITY_ID = "state_source_entity_id"
CONF_EVENT_SOURCES = "event_sources"

HacanSwitch = hacan_ns.class_("HacanSwitch", switch.Switch, cg.Component)

CONFIG_SCHEMA = (
    switch.switch_schema(HacanSwitch)
    .extend(
        {
            cv.GenerateID(CONF_HACAN_ID): cv.use_id(HacanComponent),
            cv.Required(CONF_ENTITY_ID): cv.hex_uint32_t,
            cv.Required(CONF_ENDPOINT): cv.int_range(min=1, max=0xFE),
            cv.Required(CONF_OUTPUT): cv.use_id(output.BinaryOutput),
            cv.Optional(CONF_STATE_SOURCE_ENTITY_ID): cv.All(
                cv.hex_uint32_t, cv.int_range(min=1)
            ),
            cv.Optional(CONF_EVENT_SOURCES, default=[]): EVENT_SOURCE_LIST,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await switch.new_switch(config)
    await cg.register_component(var, config)
    hacan = await cg.get_variable(config[CONF_HACAN_ID])
    output_ = await cg.get_variable(config[CONF_OUTPUT])
    cg.add(var.set_hacan(hacan))
    cg.add(var.set_output(output_))
    cg.add(var.set_endpoint(config[CONF_ENDPOINT]))
    cg.add(hacan.add_owned_endpoint(config[CONF_ENTITY_ID], config[CONF_ENDPOINT], var))
    if CONF_STATE_SOURCE_ENTITY_ID in config:
        cg.add(var.set_state_source_entity(config[CONF_STATE_SOURCE_ENTITY_ID]))
        cg.add(hacan.add_state_listener(var))
    for source in config[CONF_EVENT_SOURCES]:
        listener = var.add_event_source(
            source["entity_id"], source["event"], source["action"]
        )
        cg.add(hacan.add_event_listener(listener))
