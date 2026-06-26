#pragma once

#ifdef USE_ESP32

#include <array>
#include <vector>
#include <string>

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"

#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/select/select.h"
#include "esphome/components/light/light_state.h"

#include <esp_gattc_api.h>
#include <esp_gap_ble_api.h>

namespace esphome {
namespace sensowash_serial {

namespace espbt = esphome::esp32_ble_tracker;

// Protocol stage, independent of the underlying BLE connection state.
enum class HandshakeState : uint8_t {
  IDLE,         // not connected / no handles
  AWAITING_KEY, // wrote 00000000 challenge, waiting for the toilet to disclose its key
  READY,        // handshake complete, normal command/response traffic flowing
};

class SensowashSerial : public ble_client::BLEClientNode, public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_BLUETOOTH; }

  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;
  // GAP events are forwarded to BLE nodes; we use it to receive READ_RSSI_COMPLETE.
  void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) override;

  // ---- config from codegen ----
  void set_handshake_key(uint8_t a, uint8_t b, uint8_t c, uint8_t d);

  // ---- command API used by entity classes ----
  // Builds a framed command and pushes it onto the paced outbound queue.
  void enqueue_command(uint8_t opcode, const std::vector<uint8_t> &payload = {});
  // Dryer: index 0 = Off (stop, 0x01), 1/2/3 = Low/Warm/Hot (0x04 [temp]).
  void set_dryer_mode(uint8_t index);
  // Energy saving: packs the current seat-temperature nibble and tracks state optimistically.
  void set_energy_saving(bool state);
  void set_seat_temp_index(uint8_t index) { this->seat_temp_index_ = index; }
  uint8_t seat_temp_index() const { return this->seat_temp_index_; }

  // Washing. Params are cached and packed into the rear/lady wash start frame; the individual
  // setters also send the live-adjust opcode so a change applies to an active wash immediately.
  void set_wash_param(uint8_t kind, uint8_t index);  // 1=water flow, 2=nozzle position, 3=water temp
  void set_comfort_wash(bool state);
  void start_wash(uint8_t kind);  // 1=rear, 2=lady

  // ---- entity registration (set from codegen) ----
  void set_seat_occupied_binary_sensor(binary_sensor::BinarySensor *s) { this->seat_occupied_bsensor_ = s; }
  void set_descaling_remaining_sensor(sensor::Sensor *s) { this->descaling_remaining_sensor_ = s; }
  void set_ble_rssi_sensor(sensor::Sensor *s) { this->ble_rssi_sensor_ = s; }
  void set_serial_number_text_sensor(text_sensor::TextSensor *s) { this->serial_number_tsensor_ = s; }
  void set_software_version_text_sensor(text_sensor::TextSensor *s) { this->software_version_tsensor_ = s; }
  void set_hardware_version_text_sensor(text_sensor::TextSensor *s) { this->hardware_version_tsensor_ = s; }
  void set_descaling_state_text_sensor(text_sensor::TextSensor *s) { this->descaling_state_tsensor_ = s; }
  void set_fault_codes_text_sensor(text_sensor::TextSensor *s) { this->fault_codes_tsensor_ = s; }
  void set_pairing_status_text_sensor(text_sensor::TextSensor *s) { this->pairing_status_tsensor_ = s; }
  void set_auto_lid_open_switch(switch_::Switch *s) { this->auto_lid_open_switch_ = s; }
  void set_auto_lid_close_switch(switch_::Switch *s) { this->auto_lid_close_switch_ = s; }
  void set_beep_switch(switch_::Switch *s) { this->beep_switch_ = s; }
  void set_energy_saving_switch(switch_::Switch *s) { this->energy_saving_switch_ = s; }
  void set_night_light_select(select::Select *s) { this->night_light_select_ = s; }
  void set_dryer_select(select::Select *s) { this->dryer_select_ = s; }
  void set_seat_temperature_select(select::Select *s) { this->seat_temperature_select_ = s; }
  void set_comfort_wash_switch(switch_::Switch *s) { this->comfort_wash_switch_ = s; }
  void set_water_flow_select(select::Select *s) { this->water_flow_select_ = s; }
  void set_nozzle_position_select(select::Select *s) { this->nozzle_position_select_ = s; }
  void set_wash_water_temp_select(select::Select *s) { this->wash_water_temp_select_ = s; }
  void set_status_led(light::LightState *l) { this->status_led_ = l; }

  void add_on_connect_trigger(Trigger<> *t) { this->on_connect_triggers_.push_back(t); }
  void add_on_disconnect_trigger(Trigger<> *t) { this->on_disconnect_triggers_.push_back(t); }

 protected:
  // ---- BLE handle discovery / notify enablement ----
  bool resolve_handles_();
  void enable_notify_(uint16_t char_handle);
  void write_raw_(uint16_t handle, const std::vector<uint8_t> &data);

  // ---- handshake state machine ----
  void start_handshake_();   // called once handles + handshake notify are ready
  void maybe_finish_();      // completes handshake when key is known + notify is subscribed
  void finish_handshake_();  // mark READY, fire triggers, queue initial polls
  void send_initial_polls_();
  void store_key_(const uint8_t *key);

  // ---- frame helpers ----
  std::vector<uint8_t> build_frame_(uint8_t opcode, const std::vector<uint8_t> &payload);
  bool parse_frame_(const uint8_t *data, uint16_t len, uint8_t &opcode, std::vector<uint8_t> &payload);

  // ---- response handling ----
  void handle_notify_(const uint8_t *data, uint16_t len);
  void dispatch_opcode_(uint8_t opcode, const std::vector<uint8_t> &payload);
  void handle_time_request_();
  void parse_func_config_(const std::vector<uint8_t> &p);
  void parse_toilet_state_(const std::vector<uint8_t> &p);
  void parse_error_codes_(const std::vector<uint8_t> &p);
  void parse_descaling_state_(const std::vector<uint8_t> &p);
  void parse_descaling_remaining_(const std::vector<uint8_t> &p);
  void publish_text_(text_sensor::TextSensor *s, const std::vector<uint8_t> &p);
  void publish_pairing_(const char *state);
  void save_wash_params_();     // persist water flow / nozzle / wash temp to NVS
  void publish_wash_params_();  // reflect the persisted wash params onto the selects
  void update_led_();           // drive the onboard RGB status LED from connection/tx/fault state

  void reset_connection_();

  // ---- BLE handles ----
  uint16_t write_handle_{0};
  uint16_t notify_handle_{0};
  uint16_t handshake_handle_{0};

  // ---- handshake ----
  HandshakeState handshake_state_{HandshakeState::IDLE};
  uint8_t key_[4]{0, 0, 0, 0};
  bool key_known_{false};
  bool notify_subscribed_{false};
  uint32_t awaiting_key_since_{0};
  bool awaiting_key_logged_{false};
  ESPPreferenceObject pref_;
  // Wash params (water flow / nozzle / wash temp) aren't reported by the toilet, so we persist them.
  ESPPreferenceObject wash_pref_;

  // ---- outbound command pacing ----
  std::vector<std::vector<uint8_t>> tx_queue_;
  uint32_t last_tx_ms_{0};
  static const uint32_t TX_INTERVAL_MS = 130;

  // ---- inbound reassembly ----
  // MTU=23 splits frames >20 bytes across multiple notifications; buffer and extract
  // complete frames by the length header, resyncing on the 55 05 sync word.
  std::vector<uint8_t> rx_buffer_;

  // ---- cached state ----
  uint8_t seat_temp_index_{0};
  uint8_t last_dryer_temp_index_{0};
  bool energy_saving_optimistic_{false};
  bool energy_saving_seeded_{false};

  // ---- washing params (optimistic; toilet doesn't report them back) ----
  uint8_t water_flow_index_{1};       // 0=Low 1=Medium 2=High
  uint8_t nozzle_position_index_{2};  // 0..4
  uint8_t wash_water_temp_index_{2};  // 0..3
  bool comfort_wash_{false};

  // ---- periodic seat-state poll (toilet doesn't push occupancy) ----
  static const uint32_t SEAT_POLL_INTERVAL_MS = 30000;
  uint32_t last_seat_poll_ms_{0};

  // ---- liveness watchdog: a BLE link can die silently (no GATTC disconnect event), leaving us
  // "connected" but deaf with nothing to trigger a reconnect. Force one if no reply arrives. ----
  uint32_t last_rx_ms_{0};
  static const uint32_t RX_WATCHDOG_MS = 100000;

  // ---- one-shot toilet-state re-poll ~1s after an action command, so optimistic state (e.g. Dryer
  // when the seat is vacant) corrects against the toilet's real state instead of waiting for the 30s poll. ----
  uint32_t pending_poll_ms_{0};
  static const uint32_t POST_CMD_POLL_DELAY_MS = 1000;

  // ---- registered entities (any may be null) ----
  binary_sensor::BinarySensor *seat_occupied_bsensor_{nullptr};
  sensor::Sensor *descaling_remaining_sensor_{nullptr};
  sensor::Sensor *ble_rssi_sensor_{nullptr};
  uint32_t last_rssi_ms_{0};
  static const uint32_t RSSI_INTERVAL_MS = 10000;
  text_sensor::TextSensor *serial_number_tsensor_{nullptr};
  text_sensor::TextSensor *software_version_tsensor_{nullptr};
  text_sensor::TextSensor *hardware_version_tsensor_{nullptr};
  text_sensor::TextSensor *descaling_state_tsensor_{nullptr};
  text_sensor::TextSensor *fault_codes_tsensor_{nullptr};
  text_sensor::TextSensor *pairing_status_tsensor_{nullptr};
  switch_::Switch *auto_lid_open_switch_{nullptr};
  switch_::Switch *auto_lid_close_switch_{nullptr};
  switch_::Switch *beep_switch_{nullptr};
  switch_::Switch *energy_saving_switch_{nullptr};
  select::Select *night_light_select_{nullptr};
  select::Select *dryer_select_{nullptr};
  select::Select *seat_temperature_select_{nullptr};
  switch_::Switch *comfort_wash_switch_{nullptr};
  select::Select *water_flow_select_{nullptr};
  select::Select *nozzle_position_select_{nullptr};
  select::Select *wash_water_temp_select_{nullptr};

  // ---- onboard RGB status LED ----
  light::LightState *status_led_{nullptr};
  uint32_t led_flash_until_ms_{0};  // brief blink-off window after each TX (transmitting indicator)
  bool fault_active_{false};        // toilet has reported error codes
  bool descale_needed_{false};      // toilet reports "Descaling required" -> amber LED
  bool led_on_{false};              // last applied LED on/off (so we only re-write on change)
  uint8_t led_r_{0}, led_g_{0}, led_b_{0};
  static const uint32_t LED_TX_FLASH_MS = 80;
  static const uint32_t LED_FAULT_BLINK_MS = 400;

  std::vector<Trigger<> *> on_connect_triggers_;
  std::vector<Trigger<> *> on_disconnect_triggers_;
};

}  // namespace sensowash_serial
}  // namespace esphome

#endif  // USE_ESP32
