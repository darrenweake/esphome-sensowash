import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import ble_client, light
from esphome.const import CONF_ID, CONF_TRIGGER_ID

CODEOWNERS = []
DEPENDENCIES = ["ble_client"]
# Pull in the base platform components so this component's headers always link, even if a
# particular platform block is omitted from the user's YAML.
AUTO_LOAD = ["binary_sensor", "sensor", "text_sensor", "switch", "select", "button"]
MULTI_CONF = True

CONF_HANDSHAKE_KEY = "handshake_key"
CONF_STATUS_LED = "status_led"
CONF_ON_CONNECT = "on_connect"
CONF_ON_DISCONNECT = "on_disconnect"

# Used by the platform __init__.py files to reference the hub.
CONF_SENSOWASH_SERIAL_ID = "sensowash_serial_id"

sensowash_ns = cg.esphome_ns.namespace("sensowash_serial")
SensowashSerial = sensowash_ns.class_(
    "SensowashSerial", cg.Component, ble_client.BLEClientNode
)


def _validate_key(value):
    value = cv.string_strict(value)
    cleaned = value.replace(":", "").replace(" ", "").replace("0x", "").lower()
    if len(cleaned) != 8:
        raise cv.Invalid("handshake_key must be exactly 4 bytes (8 hex characters)")
    try:
        return [int(cleaned[i : i + 2], 16) for i in range(0, 8, 2)]
    except ValueError as err:
        raise cv.Invalid("handshake_key must be valid hexadecimal") from err


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SensowashSerial),
            cv.Optional(CONF_HANDSHAKE_KEY): _validate_key,
            cv.Optional(CONF_STATUS_LED): cv.use_id(light.LightState),
            cv.Optional(CONF_ON_CONNECT): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        automation.Trigger.template()
                    ),
                }
            ),
            cv.Optional(CONF_ON_DISCONNECT): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        automation.Trigger.template()
                    ),
                }
            ),
        }
    )
    .extend(ble_client.BLE_CLIENT_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await ble_client.register_ble_node(var, config)

    if CONF_HANDSHAKE_KEY in config:
        k = config[CONF_HANDSHAKE_KEY]
        cg.add(var.set_handshake_key(k[0], k[1], k[2], k[3]))

    if CONF_STATUS_LED in config:
        led = await cg.get_variable(config[CONF_STATUS_LED])
        cg.add(var.set_status_led(led))

    for conf in config.get(CONF_ON_CONNECT, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
        await automation.build_automation(trigger, [], conf)
        cg.add(var.add_on_connect_trigger(trigger))

    for conf in config.get(CONF_ON_DISCONNECT, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
        await automation.build_automation(trigger, [], conf)
        cg.add(var.add_on_disconnect_trigger(trigger))
