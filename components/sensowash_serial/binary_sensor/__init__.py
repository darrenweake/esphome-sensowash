import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import DEVICE_CLASS_OCCUPANCY

from .. import CONF_SENSOWASH_SERIAL_ID, SensowashSerial

DEPENDENCIES = ["sensowash_serial"]

CONF_SEAT_OCCUPIED = "seat_occupied"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_SENSOWASH_SERIAL_ID): cv.use_id(SensowashSerial),
        cv.Optional(CONF_SEAT_OCCUPIED): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_OCCUPANCY,
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_SENSOWASH_SERIAL_ID])
    if CONF_SEAT_OCCUPIED in config:
        bs = await binary_sensor.new_binary_sensor(config[CONF_SEAT_OCCUPIED])
        cg.add(parent.set_seat_occupied_binary_sensor(bs))
