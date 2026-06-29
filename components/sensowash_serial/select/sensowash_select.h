#pragma once

#ifdef USE_ESP32

#include <string>

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/select/select.h"
#include "../sensowash_serial.h"

namespace esphome {
namespace sensowash_serial {

// Index-mapped select: sends opcode with [selected index] payload.
// Used for Night Light (0x41) and Seat Temperature (0x25).
class SensowashIndexSelect : public select::Select, public Parented<SensowashSerial> {
 public:
  void set_opcode(uint8_t opcode) { this->opcode_ = opcode; }
  void set_is_seat_temp(bool v) { this->seat_temp_ = v; }
  void set_wash_param(uint8_t kind) { this->wash_param_ = kind; }  // 1=flow 2=nozzle 3=water-temp

 protected:
  void control(const std::string &value) override {
    auto idx = this->index_of(value);
    if (!idx.has_value())
      return;
    if (this->wash_param_ != 0) {
      // Hub sends the live-adjust opcode and caches the value for the next rear/lady wash start.
      this->parent_->set_wash_param(this->wash_param_, (uint8_t) *idx);
      this->publish_state(value);
      return;
    }
    this->parent_->enqueue_command(this->opcode_, {(uint8_t) *idx});
    this->publish_state(value);
    if (this->seat_temp_)
      this->parent_->set_seat_temp_index((uint8_t) *idx);
  }

  uint8_t opcode_{0};
  bool seat_temp_{false};
  uint8_t wash_param_{0};
};

// Dryer select: Off -> stop (0x01); Low/Warm/Hot -> 0x04 [1/2/3]. Handled in the hub.
class SensowashDryerSelect : public select::Select, public Parented<SensowashSerial> {
 protected:
  void control(const std::string &value) override {
    auto idx = this->index_of(value);
    if (!idx.has_value())
      return;
    this->parent_->set_dryer_mode((uint8_t) *idx);
    this->publish_state(value);
  }
};

}  // namespace sensowash_serial
}  // namespace esphome

#endif  // USE_ESP32
