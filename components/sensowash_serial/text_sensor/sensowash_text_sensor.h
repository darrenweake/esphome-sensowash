#pragma once

// The text_sensor platform uses ESPHome's base text_sensor::TextSensor type directly; the
// SensowashSerial hub holds pointers (serial number, software/hardware version, descaling state,
// fault codes) and publishes parsed strings. No subclass is required.

#include "esphome/components/text_sensor/text_sensor.h"
