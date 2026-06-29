#pragma once

// The sensor platform uses ESPHome's base sensor::Sensor type directly; the SensowashSerial hub
// holds a pointer to it (set_descaling_remaining_sensor) and publishes parsed values. No subclass
// is required, so this header exists only to mirror the component layout.

#include "esphome/components/sensor/sensor.h"
