import esphome.codegen as cg
from esphome.components import output, switch
import esphome.config_validation as cv

from .. import HacanComponent, hacan_ns

DEPENDENCIES = ["hacan"]

CONF_HACAN_ID = "hacan_id"
CONF_ENTITY_ID = "entity_id"
CONF_ENDPOINT = "endpoint"
CONF_OUTPUT = "output"

HacanSwitch = hacan_ns.class_("HacanSwitch", switch.Switch, cg.Component)

CONFIG_SCHEMA = (
    switch.switch_schema(HacanSwitch)
    .extend(
        {
            cv.GenerateID(CONF_HACAN_ID): cv.use_id(HacanComponent),
            cv.Required(CONF_ENTITY_ID): cv.hex_uint32_t,
            cv.Required(CONF_ENDPOINT): cv.int_range(min=1, max=0xFE),
            cv.Required(CONF_OUTPUT): cv.use_id(output.BinaryOutput),
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
