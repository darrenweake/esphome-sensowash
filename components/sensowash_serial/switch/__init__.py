import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import ENTITY_CATEGORY_CONFIG

from .. import CONF_SENSOWASH_SERIAL_ID, SensowashSerial, sensowash_ns

DEPENDENCIES = ["sensowash_serial"]

SensowashSwitch = sensowash_ns.class_("SensowashSwitch", switch.Switch)

# key -> (opcode, energy_saving?, parent setter name)
SWITCHES = {
    "auto_lid_open": (0x43, False, "set_auto_lid_open_switch"),
    "auto_lid_close": (0x44, False, "set_auto_lid_close_switch"),
    "beep": (0x40, False, "set_beep_switch"),
    "energy_saving": (0x47, True, "set_energy_saving_switch"),
}

CONF_COMFORT_WASH = "comfort_wash"
CONF_HOLIDAY_MODE = "holiday_mode"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_SENSOWASH_SERIAL_ID): cv.use_id(SensowashSerial),
        **{
            cv.Optional(key): switch.switch_schema(
                SensowashSwitch,
                entity_category=ENTITY_CATEGORY_CONFIG,
            )
            for key in SWITCHES
        },
        # Comfort (oscillating) wash is a runtime control, not a config switch.
        cv.Optional(CONF_COMFORT_WASH): switch.switch_schema(SensowashSwitch),
        # Holiday mode: ON drains the tank and is tracked optimistically (the toilet never reports
        # it back); auto-exits on next seat use. A runtime control, not a config switch.
        cv.Optional(CONF_HOLIDAY_MODE): switch.switch_schema(SensowashSwitch),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_SENSOWASH_SERIAL_ID])
    for key, (opcode, is_energy, setter) in SWITCHES.items():
        if key not in config:
            continue
        sw = await switch.new_switch(config[key])
        await cg.register_parented(sw, parent)
        cg.add(sw.set_opcode(opcode))
        if is_energy:
            cg.add(sw.set_energy_saving(True))
        cg.add(getattr(parent, setter)(sw))

    if CONF_COMFORT_WASH in config:
        sw = await switch.new_switch(config[CONF_COMFORT_WASH])
        await cg.register_parented(sw, parent)
        cg.add(sw.set_comfort(True))
        cg.add(parent.set_comfort_wash_switch(sw))  # so func-config readback can reflect it

    if CONF_HOLIDAY_MODE in config:
        sw = await switch.new_switch(config[CONF_HOLIDAY_MODE])
        await cg.register_parented(sw, parent)
        cg.add(sw.set_holiday(True))
        cg.add(parent.set_holiday_switch(sw))  # so auto-exit can clear it
