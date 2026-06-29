import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    DEVICE_CLASS_SIGNAL_STRENGTH,
    STATE_CLASS_MEASUREMENT,
    UNIT_DECIBEL_MILLIWATT,
    ENTITY_CATEGORY_DIAGNOSTIC,
)

from .. import CONF_SENSOWASH_SERIAL_ID, SensowashSerial

DEPENDENCIES = ["sensowash_serial"]

CONF_DESCALING_REMAINING = "descaling_remaining"
CONF_BLE_RSSI = "ble_rssi"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_SENSOWASH_SERIAL_ID): cv.use_id(SensowashSerial),
        cv.Optional(CONF_DESCALING_REMAINING): sensor.sensor_schema(
            unit_of_measurement="d",
            accuracy_decimals=0,
            icon="mdi:calendar-clock",
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        # RSSI of the BLE link to the toilet (read periodically while connected).
        cv.Optional(CONF_BLE_RSSI): sensor.sensor_schema(
            unit_of_measurement=UNIT_DECIBEL_MILLIWATT,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_SENSOWASH_SERIAL_ID])
    if CONF_DESCALING_REMAINING in config:
        s = await sensor.new_sensor(config[CONF_DESCALING_REMAINING])
        cg.add(parent.set_descaling_remaining_sensor(s))
    if CONF_BLE_RSSI in config:
        s = await sensor.new_sensor(config[CONF_BLE_RSSI])
        cg.add(parent.set_ble_rssi_sensor(s))
