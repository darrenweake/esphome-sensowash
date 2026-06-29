import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import select
from esphome.const import ENTITY_CATEGORY_CONFIG

from .. import CONF_SENSOWASH_SERIAL_ID, SensowashSerial, sensowash_ns

DEPENDENCIES = ["sensowash_serial"]

SensowashIndexSelect = sensowash_ns.class_(
    "SensowashIndexSelect", select.Select
)
SensowashDryerSelect = sensowash_ns.class_(
    "SensowashDryerSelect", select.Select
)

CONF_NIGHT_LIGHT = "night_light"
CONF_DRYER = "dryer"
CONF_SEAT_TEMPERATURE = "seat_temperature"
CONF_WATER_FLOW = "water_flow"
CONF_NOZZLE_POSITION = "nozzle_position"
CONF_WASH_WATER_TEMPERATURE = "wash_water_temperature"

NIGHT_LIGHT_OPTIONS = ["Off", "On", "Auto"]
TEMP_OPTIONS = ["Off", "Low", "Warm", "Hot"]
WATER_FLOW_OPTIONS = ["Low", "Medium", "High"]
NOZZLE_OPTIONS = ["1", "2", "3", "4", "5"]
WASH_TEMP_OPTIONS = ["Off", "Low", "Medium", "High"]

# wash-param selects: key -> (options, hub kind 1=flow 2=nozzle 3=water-temp)
WASH_SELECTS = {
    CONF_WATER_FLOW: (WATER_FLOW_OPTIONS, 1),
    CONF_NOZZLE_POSITION: (NOZZLE_OPTIONS, 2),
    CONF_WASH_WATER_TEMPERATURE: (WASH_TEMP_OPTIONS, 3),
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_SENSOWASH_SERIAL_ID): cv.use_id(SensowashSerial),
        cv.Optional(CONF_NIGHT_LIGHT): select.select_schema(
            SensowashIndexSelect, entity_category=ENTITY_CATEGORY_CONFIG
        ),
        cv.Optional(CONF_DRYER): select.select_schema(SensowashDryerSelect),
        cv.Optional(CONF_SEAT_TEMPERATURE): select.select_schema(
            SensowashIndexSelect, entity_category=ENTITY_CATEGORY_CONFIG
        ),
        **{
            cv.Optional(key): select.select_schema(SensowashIndexSelect)
            for key in WASH_SELECTS
        },
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_SENSOWASH_SERIAL_ID])

    if CONF_NIGHT_LIGHT in config:
        sel = await select.new_select(
            config[CONF_NIGHT_LIGHT], options=NIGHT_LIGHT_OPTIONS
        )
        await cg.register_parented(sel, parent)
        cg.add(sel.set_opcode(0x41))
        cg.add(parent.set_night_light_select(sel))

    if CONF_SEAT_TEMPERATURE in config:
        sel = await select.new_select(
            config[CONF_SEAT_TEMPERATURE], options=TEMP_OPTIONS
        )
        await cg.register_parented(sel, parent)
        cg.add(sel.set_opcode(0x25))
        cg.add(sel.set_is_seat_temp(True))
        cg.add(parent.set_seat_temperature_select(sel))

    if CONF_DRYER in config:
        sel = await select.new_select(config[CONF_DRYER], options=TEMP_OPTIONS)
        await cg.register_parented(sel, parent)
        cg.add(parent.set_dryer_select(sel))

    wash_select_setters = {
        1: "set_water_flow_select",
        2: "set_nozzle_position_select",
        3: "set_wash_water_temp_select",
    }
    for key, (opts, kind) in WASH_SELECTS.items():
        if key in config:
            sel = await select.new_select(config[key], options=opts)
            await cg.register_parented(sel, parent)
            cg.add(sel.set_wash_param(kind))
            cg.add(getattr(parent, wash_select_setters[kind])(sel))  # so persisted values show on connect
