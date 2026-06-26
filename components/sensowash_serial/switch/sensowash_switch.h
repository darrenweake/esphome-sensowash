#pragma once

#ifdef USE_ESP32

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/switch/switch.h"
#include "../sensowash_serial.h"

namespace esphome {
namespace sensowash_serial {

// Generic on/off switch bound to an opcode with a single [0/1] payload byte.
// The energy-saving switch is special-cased (packed seat-temp nibble + optimistic tracking).
class SensowashSwitch : public switch_::Switch, public Parented<SensowashSerial> {
 public:
  void set_opcode(uint8_t opcode) { this->opcode_ = opcode; }
  void set_energy_saving(bool v) { this->energy_saving_ = v; }
  void set_comfort(bool v) { this->comfort_ = v; }

 protected:
  void write_state(bool state) override {
    if (this->energy_saving_) {
      this->parent_->set_energy_saving(state);
      return;
    }
    if (this->comfort_) {
      this->parent_->set_comfort_wash(state);
      this->publish_state(state);
      return;
    }
    this->parent_->enqueue_command(this->opcode_, {(uint8_t) (state ? 0x01 : 0x00)});
    this->publish_state(state);
  }

  uint8_t opcode_{0};
  bool energy_saving_{false};
  bool comfort_{false};
};

}  // namespace sensowash_serial
}  // namespace esphome

#endif  // USE_ESP32
