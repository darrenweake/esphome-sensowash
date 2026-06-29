import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC

from .. import CONF_SENSOWASH_SERIAL_ID, SensowashSerial

DEPENDENCIES = ["sensowash_serial"]

# key -> parent setter name
TEXT_SENSORS = {
    "serial_number": "set_serial_number_text_sensor",
    "software_version": "set_software_version_text_sensor",
    "hardware_version": "set_hardware_version_text_sensor",
    "descaling_state": "set_descaling_state_text_sensor",
    "fault_codes": "set_fault_codes_text_sensor",
    "pairing_status": "set_pairing_status_text_sensor",
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_SENSOWASH_SERIAL_ID): cv.use_id(SensowashSerial),
        **{
            cv.Optional(key): text_sensor.text_sensor_schema(
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            )
            for key in TEXT_SENSORS
        },
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_SENSOWASH_SERIAL_ID])
    for key, setter in TEXT_SENSORS.items():
        if key not in config:
            continue
        ts = await text_sensor.new_text_sensor(config[key])
        cg.add(getattr(parent, setter)(ts))
