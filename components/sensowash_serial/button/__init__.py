import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button
from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC

from .. import CONF_SENSOWASH_SERIAL_ID, SensowashSerial, sensowash_ns

DEPENDENCIES = ["sensowash_serial"]

SensowashButton = sensowash_ns.class_("SensowashButton", button.Button)

# key -> (opcode, static payload, diagnostic?)
BUTTONS = {
    "lid_toggle": (0x09, [], False),
    "wash_stop": (0x01, [], False),
    "nozzle_self_clean": (0x60, [], True),
    "nozzle_manual_clean": (0x61, [], True),
    # holiday_mode (0x62 [01]) is now a stateful switch on the switch platform.
    "tank_drain_en1717": (0x62, [0x00], True),
    "descaling": (0x63, [], True),
}

# Wash-start buttons: dynamic payload packed from the hub's cached params. key -> wash kind.
WASH_BUTTONS = {
    "rear_wash": 1,
    "lady_wash": 2,
}

def _button_schema(diag):
    kwargs = {}
    if diag:
        kwargs["entity_category"] = ENTITY_CATEGORY_DIAGNOSTIC
    return button.button_schema(SensowashButton, **kwargs)


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_SENSOWASH_SERIAL_ID): cv.use_id(SensowashSerial),
        **{
            cv.Optional(key): _button_schema(diag)
            for key, (_opcode, _payload, diag) in BUTTONS.items()
        },
        **{cv.Optional(key): _button_schema(False) for key in WASH_BUTTONS},
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_SENSOWASH_SERIAL_ID])
    for key, (opcode, payload, _diag) in BUTTONS.items():
        if key not in config:
            continue
        b = await button.new_button(config[key])
        await cg.register_parented(b, parent)
        cg.add(b.set_opcode(opcode))
        for byte in payload:
            cg.add(b.add_payload_byte(byte))
    for key, kind in WASH_BUTTONS.items():
        if key not in config:
            continue
        b = await button.new_button(config[key])
        await cg.register_parented(b, parent)
        cg.add(b.set_wash(kind))
