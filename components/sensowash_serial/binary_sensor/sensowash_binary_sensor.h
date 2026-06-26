#pragma once

// The binary_sensor platform uses ESPHome's base binary_sensor::BinarySensor type directly; the
// SensowashSerial hub holds a pointer (set_seat_occupied_binary_sensor) and publishes the parsed
// occupancy state. No subclass is required.

#include "esphome/components/binary_sensor/binary_sensor.h"
