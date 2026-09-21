import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import canbus
from esphome.const import CONF_ID

CODEOWNERS = ["@morbic"]
DEPENDENCIES = ["canbus"]
MULTI_CONF = True

CONF_CANBUS_ID = "canbus_id"
CONF_COMMISSIONING_ROLE = "commissioning_role"
CONF_HACAN_ID = "hacan_id"
CONF_ENTITY_ID = "entity_id"
CONF_ENDPOINT = "endpoint"
CONF_EVENT = "event"

hacan_ns = cg.esphome_ns.namespace("hacan_esphome")
HacanComponent = hacan_ns.class_("HacanComponent", cg.Component)
PublishEventAction = hacan_ns.class_("PublishEventAction", automation.Action)

ROLE = {"primary": 0, "secondary": 1, "none": 2}
BUTTON_EVENT = {
    "press": 0x01,
    "release": 0x02,
    "click": 0x03,
    "double_click": 0x04,
    "long_press": 0x05,
}
EVENT_ACTION = {"toggle": 0, "turn_on": 1, "turn_off": 2}
EVENT_SOURCE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ENTITY_ID): cv.hex_uint32_t,
        cv.Required(CONF_EVENT): cv.enum(BUTTON_EVENT, lower=True),
        cv.Required("action"): cv.enum(EVENT_ACTION, lower=True),
    }
)
EVENT_SOURCE_LIST = cv.All(cv.ensure_list(EVENT_SOURCE_SCHEMA), cv.Length(max=8))

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


PUBLISH_EVENT_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_HACAN_ID): cv.use_id(HacanComponent),
        cv.Required(CONF_ENTITY_ID): cv.hex_uint32_t,
        cv.Required(CONF_ENDPOINT): cv.int_range(min=1, max=0xFE),
        cv.Required(CONF_EVENT): cv.enum(BUTTON_EVENT, lower=True),
    }
)


@automation.register_action(
    "hacan.publish_event", PublishEventAction, PUBLISH_EVENT_SCHEMA, synchronous=True
)
async def hacan_publish_event_to_code(config, action_id, template_arg, args):
    hacan = await cg.get_variable(config[CONF_HACAN_ID])
    var = cg.new_Pvariable(action_id, template_arg)
    cg.add(var.set_hacan(hacan))
    cg.add(var.set_endpoint(config[CONF_ENDPOINT]))
    cg.add(var.set_event(config[CONF_EVENT]))
    cg.add(hacan.add_owned_endpoint(config[CONF_ENTITY_ID], config[CONF_ENDPOINT], cg.nullptr))
    return var
