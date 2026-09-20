import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import canbus
from esphome.const import CONF_ID

CODEOWNERS = ["@morbic"]
DEPENDENCIES = ["canbus"]
MULTI_CONF = True

CONF_CANBUS_ID = "canbus_id"
CONF_COMMISSIONING_ROLE = "commissioning_role"
CONF_OWNED_ENTITIES = "owned_entities"
CONF_OBSERVED_ENTITIES = "observed_entities"
CONF_ENDPOINT = "endpoint"
CONF_ENTITY_ID = "entity_id"

hacan_ns = cg.esphome_ns.namespace("hacan_esphome")
HacanComponent = hacan_ns.class_("HacanComponent", cg.Component)

ROLE = {"primary": 0, "secondary": 1, "none": 2}
ENTITY = cv.Schema({
    cv.Required(CONF_ENTITY_ID): cv.hex_uint32_t,
    cv.Optional(CONF_ENDPOINT): cv.hex_uint8_t,
})
ENTITY_LIST = cv.All(cv.ensure_list(ENTITY), cv.Length(max=16))
OWNED_ENTITY_LIST = cv.All(
    cv.ensure_list(ENTITY.extend({cv.Required(CONF_ENDPOINT): cv.hex_uint8_t})),
    cv.Length(max=16),
)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(HacanComponent),
    cv.Required(CONF_CANBUS_ID): cv.use_id(canbus.CanbusComponent),
    cv.Optional(CONF_COMMISSIONING_ROLE, default="none"): cv.enum(ROLE, lower=True),
    cv.Optional(CONF_OWNED_ENTITIES, default=[]): OWNED_ENTITY_LIST,
    cv.Optional(CONF_OBSERVED_ENTITIES, default=[]): ENTITY_LIST,
}).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    bus = await cg.get_variable(config[CONF_CANBUS_ID])
    cg.add(var.set_canbus(bus))
    cg.add(var.set_commissioning_role(config[CONF_COMMISSIONING_ROLE]))
    for entity in config[CONF_OWNED_ENTITIES]:
        cg.add(var.add_owned_entity(entity[CONF_ENTITY_ID], entity[CONF_ENDPOINT]))
    for entity in config[CONF_OBSERVED_ENTITIES]:
        cg.add(var.add_observed_entity(entity[CONF_ENTITY_ID]))
