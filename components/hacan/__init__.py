import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import canbus
from esphome.const import CONF_ID

CODEOWNERS = ["@morbic"]
DEPENDENCIES = ["canbus"]
MULTI_CONF = True

CONF_CANBUS_ID = "canbus_id"
CONF_COMMISSIONING_ROLE = "commissioning_role"

hacan_ns = cg.esphome_ns.namespace("hacan_esphome")
HacanComponent = hacan_ns.class_("HacanComponent", cg.Component)

ROLE = {"primary": 0, "secondary": 1, "none": 2}

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(HacanComponent),
    cv.Required(CONF_CANBUS_ID): cv.use_id(canbus.CanbusComponent),
    cv.Optional(CONF_COMMISSIONING_ROLE, default="none"): cv.enum(ROLE, lower=True),
}).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    bus = await cg.get_variable(config[CONF_CANBUS_ID])
    cg.add(var.set_canbus(bus))
    cg.add(var.set_commissioning_role(config[CONF_COMMISSIONING_ROLE]))
