#pragma once

#ifdef USE_ESP32

#include <vector>

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/button/button.h"
#include "../sensowash_serial.h"

namespace esphome {
namespace sensowash_serial {

// A one-shot button bound to a fixed opcode (+ optional static payload).
class SensowashButton : public button::Button, public Parented<SensowashSerial> {
 public:
  void set_opcode(uint8_t opcode) { this->opcode_ = opcode; }
  void add_payload_byte(uint8_t b) { this->payload_.push_back(b); }
  void set_wash(uint8_t kind) { this->wash_kind_ = kind; }  // 1=rear, 2=lady (dynamic packed payload)

 protected:
  void press_action() override {
    if (this->wash_kind_ != 0) {
      this->parent_->start_wash(this->wash_kind_);
      return;
    }
    this->parent_->enqueue_command(this->opcode_, this->payload_);
  }

  uint8_t opcode_{0};
  uint8_t wash_kind_{0};
  std::vector<uint8_t> payload_;
};

}  // namespace sensowash_serial
}  // namespace esphome

#endif  // USE_ESP32
