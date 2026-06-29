#include "sensowash_serial.h"

#ifdef USE_ESP32

#include <cstring>
#include <ctime>

#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/application.h"  // App.safe_reboot() for forget_key()

namespace esphome {
namespace sensowash_serial {

static const char *const TAG = "sensowash_serial";

// 128-bit GATT UUIDs (reverse-engineered, proven).
static const char *const SERVICE_UUID = "00011111-0405-0607-0809-0a0b0c0d11ff";
static const char *const WRITE_UUID = "00013333-0405-0607-0809-0a0b0c0d11ff";
static const char *const NOTIFY_UUID = "00012222-0405-0607-0809-0a0b0c0d11ff";
static const char *const HANDSHAKE_UUID = "00014444-0405-0607-0809-0a0b0c0d11ff";

// Command opcodes (written to WRITE_UUID). Per-entity opcodes (lid, night light, seat temp, …)
// are supplied from codegen; only the ones the hub issues directly are named here.
static const uint8_t OP_STOP = 0x01;
static const uint8_t OP_REAR_WASH = 0x02;
static const uint8_t OP_LADY_WASH = 0x03;
static const uint8_t OP_DRYER = 0x04;
static const uint8_t OP_COMFORT_WASH = 0x20;
static const uint8_t OP_NOZZLE_POSITION = 0x21;
static const uint8_t OP_WATER_FLOW = 0x22;
static const uint8_t OP_WATER_TEMP = 0x23;
static const uint8_t OP_TIME_REPLY = 0x2B;
static const uint8_t OP_ENERGY_SAVING = 0x47;

// Request opcodes used on connect.
static const uint8_t OP_REQ_FUNC_CONFIG = 0x50;
static const uint8_t OP_REQ_TOILET_STATE = 0x52;
static const uint8_t OP_REQ_ERROR_CODES = 0x54;
static const uint8_t OP_REQ_SW_VERSION = 0x56;
static const uint8_t OP_REQ_SERIAL = 0x58;
static const uint8_t OP_REQ_HW_VERSION = 0x5A;
static const uint8_t OP_REQ_DESCALING_STATE = 0x64;
static const uint8_t OP_REQ_DESCALING_REMAINING = 0x69;

// Notify opcodes (received on NOTIFY_UUID).
static const uint8_t OP_TIME_REQUEST = 0x2A;
static const uint8_t OP_RESP_FUNC_CONFIG = 0x51;
static const uint8_t OP_RESP_TOILET_STATE = 0x53;
static const uint8_t OP_RESP_ERROR_CODES = 0x55;
static const uint8_t OP_RESP_SW_VERSION = 0x57;
static const uint8_t OP_RESP_SERIAL = 0x59;
static const uint8_t OP_RESP_HW_VERSION = 0x5B;
static const uint8_t OP_RESP_DESCALING_STATE = 0x65;
static const uint8_t OP_RESP_DESCALING_REMAINING = 0x66;

static const uint32_t HANDSHAKE_TIMEOUT_MS = 15000;

// Extract bits [start..end] (inclusive, LSB=0) from a byte.
static inline uint8_t bits(uint8_t byte, uint8_t start, uint8_t end) {
  uint8_t mask = (1 << (end - start + 1)) - 1;
  return (byte >> start) & mask;
}

void SensowashSerial::set_handshake_key(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
  this->key_[0] = a;
  this->key_[1] = b;
  this->key_[2] = c;
  this->key_[3] = d;
  this->key_known_ = true;
}

void SensowashSerial::forget_key() {
  // Write an all-0xFF sentinel (the erased-flash pattern; not a valid key) so the next boot enrolls
  // afresh, then reboot. Reachable only via the internal REST-only "Forget Pairing Key" button.
  std::array<uint8_t, 4> sentinel{0xFF, 0xFF, 0xFF, 0xFF};
  this->pref_.save(&sentinel);
  global_preferences->sync();
  ESP_LOGW(TAG, "Pairing key cleared from NVS; rebooting to re-enroll on next connect");
  delay(100);  // let the log line flush before the reboot
  App.safe_reboot();
}

void SensowashSerial::setup() {
  // A stored (learned) key takes precedence over a configured default; both end up identical
  // for a given toilet, but a learned key proves the enrollment actually happened.
  this->pref_ = global_preferences->make_preference<std::array<uint8_t, 4>>(fnv1_hash("sensowash_serial_key"));
  std::array<uint8_t, 4> stored{};
  if (this->pref_.load(&stored)) {
    if (stored[0] == 0xFF && stored[1] == 0xFF && stored[2] == 0xFF && stored[3] == 0xFF) {
      // Sentinel written by forget_key(): pairing was deliberately cleared -> enroll on next connect
      // (this also overrides any configured handshake_key, matching "stored takes precedence").
      this->key_known_ = false;
      ESP_LOGI(TAG, "Stored handshake key was cleared; will enroll on next connect");
    } else {
      std::memcpy(this->key_, stored.data(), 4);
      this->key_known_ = true;
      ESP_LOGI(TAG, "Loaded stored handshake key %02x%02x%02x%02x", this->key_[0], this->key_[1], this->key_[2],
               this->key_[3]);
    }
  }

  // Wash params aren't reported by the toilet (the official app keeps them locally too), so persist
  // our own copy across reboots instead of resetting to defaults.
  this->wash_pref_ = global_preferences->make_preference<std::array<uint8_t, 3>>(fnv1_hash("sensowash_wash_params"));
  std::array<uint8_t, 3> wp{};
  if (this->wash_pref_.load(&wp)) {
    if (wp[0] <= 2) this->water_flow_index_ = wp[0];
    if (wp[1] <= 4) this->nozzle_position_index_ = wp[1];
    if (wp[2] <= 3) this->wash_water_temp_index_ = wp[2];
    ESP_LOGI(TAG, "Loaded wash params flow=%u nozzle=%u temp=%u", wp[0], wp[1], wp[2]);
  }
}

void SensowashSerial::dump_config() {
  ESP_LOGCONFIG(TAG, "SensoWash Serial:");
  if (this->key_known_) {
    ESP_LOGCONFIG(TAG, "  Handshake key: %02x%02x%02x%02x (known)", this->key_[0], this->key_[1], this->key_[2],
                  this->key_[3]);
  } else {
    ESP_LOGCONFIG(TAG, "  Handshake key: unknown (will enroll on first connect)");
  }
}

void SensowashSerial::loop() {
  // Enrollment timeout warning (does not abort — the toilet can take several seconds).
  if (this->handshake_state_ == HandshakeState::AWAITING_KEY && !this->awaiting_key_logged_ &&
      (millis() - this->awaiting_key_since_) > HANDSHAKE_TIMEOUT_MS) {
    ESP_LOGW(TAG, "Still waiting for handshake key after %us; the toilet may be slow to disclose it",
             (unsigned) (HANDSHAKE_TIMEOUT_MS / 1000));
    this->awaiting_key_logged_ = true;
  }

  // Paced outbound command queue. The toilet's serial bridge drops back-to-back writes.
  if (this->handshake_state_ == HandshakeState::READY && !this->tx_queue_.empty() &&
      (millis() - this->last_tx_ms_) >= TX_INTERVAL_MS) {
    auto frame = this->tx_queue_.front();
    this->tx_queue_.erase(this->tx_queue_.begin());
    ESP_LOGI(TAG, "TX %s", format_hex_pretty(frame).c_str());
    this->write_raw_(this->write_handle_, frame);
    this->last_tx_ms_ = millis();
    this->led_flash_until_ms_ = millis() + LED_TX_FLASH_MS;  // blink the status LED while transmitting
  }

  // Periodically sample the BLE link RSSI while connected.
  if (this->handshake_state_ == HandshakeState::READY && this->ble_rssi_sensor_ != nullptr &&
      (millis() - this->last_rssi_ms_) >= RSSI_INTERVAL_MS) {
    this->last_rssi_ms_ = millis();
    esp_ble_gap_read_rssi(this->parent()->get_remote_bda());
  }

  // The toilet never pushes seat occupancy, so re-request toilet-state on a slow cadence to keep the
  // Seat Occupied sensor live. 30s is ~30x gentler than the official app's 1s wash poll.
  if (this->handshake_state_ == HandshakeState::READY &&
      (millis() - this->last_seat_poll_ms_) >= SEAT_POLL_INTERVAL_MS) {
    this->last_seat_poll_ms_ = millis();
    this->enqueue_command(OP_REQ_TOILET_STATE);
  }

  // One-shot re-poll shortly after an action command so optimistically-shown state (e.g. Dryer set
  // while the seat is vacant) corrects against the toilet's real state within ~1s, not 30s.
  if (this->handshake_state_ == HandshakeState::READY && this->pending_poll_ms_ != 0 &&
      (int32_t) (millis() - this->pending_poll_ms_) >= 0) {
    this->pending_poll_ms_ = 0;
    this->enqueue_command(OP_REQ_TOILET_STATE);
  }

  // Liveness watchdog (see header): if READY but no reply for RX_WATCHDOG_MS, the link has likely died
  // silently with no disconnect event — force a disconnect so ESPHome's auto-reconnect recovers it.
  if (this->handshake_state_ == HandshakeState::READY &&
      (millis() - this->last_rx_ms_) >= RX_WATCHDOG_MS) {
    ESP_LOGW(TAG, "No toilet reply for %us while connected — forcing reconnect (silent-link watchdog)",
             (unsigned) (RX_WATCHDOG_MS / 1000));
    this->last_rx_ms_ = millis();  // prevent immediate re-trigger before the disconnect lands
    this->parent()->disconnect();
  }

  this->update_led_();
}

void SensowashSerial::gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  if (event == ESP_GAP_BLE_READ_RSSI_COMPLETE_EVT) {
    auto &r = param->read_rssi_cmpl;
    if (r.status == ESP_BT_STATUS_SUCCESS && this->ble_rssi_sensor_ != nullptr)
      this->ble_rssi_sensor_->publish_state(r.rssi);
  }
}

// ---------------------------------------------------------------------------
// Frame helpers
// ---------------------------------------------------------------------------

std::vector<uint8_t> SensowashSerial::build_frame_(uint8_t opcode, const std::vector<uint8_t> &payload) {
  uint8_t len = payload.size() + 3;
  uint32_t sum = len + opcode;
  for (auto b : payload)
    sum += b;
  std::vector<uint8_t> frame = {0x55, 0x05, len, opcode};
  frame.insert(frame.end(), payload.begin(), payload.end());
  frame.push_back(sum & 0xFF);
  return frame;
}

bool SensowashSerial::parse_frame_(const uint8_t *data, uint16_t data_len, uint8_t &opcode,
                                   std::vector<uint8_t> &payload) {
  if (data_len < 5 || data[0] != 0x55)
    return false;
  uint8_t len = data[2];
  if (data_len < (uint16_t) (len + 2))
    return false;
  opcode = data[3];
  payload.assign(data + 4, data + len + 1);
  uint32_t sum = len + opcode;
  for (auto b : payload)
    sum += b;
  return (sum & 0xFF) == data[len + 1];
}

// ---------------------------------------------------------------------------
// Command API
// ---------------------------------------------------------------------------

void SensowashSerial::enqueue_command(uint8_t opcode, const std::vector<uint8_t> &payload) {
  if (this->handshake_state_ != HandshakeState::READY) {
    ESP_LOGW(TAG, "Dropping command 0x%02X: not connected", opcode);
    return;
  }
  this->tx_queue_.push_back(this->build_frame_(opcode, payload));
}

void SensowashSerial::set_dryer_mode(uint8_t index) {
  if (index == 0) {
    this->enqueue_command(OP_STOP);
  } else {
    this->enqueue_command(OP_DRYER, {index});
  }
  this->last_dryer_temp_index_ = index;
  if (this->dryer_select_ != nullptr) {
    auto opt = this->dryer_select_->at(index);
    if (opt.has_value())
      this->dryer_select_->publish_state(*opt);
  }
  this->pending_poll_ms_ = millis() + POST_CMD_POLL_DELAY_MS;  // re-check real state (toilet ignores it if seat vacant)
}

void SensowashSerial::set_energy_saving(bool state) {
  // Schedule-based: a window = the period when seat heating is automatically OFF (eco). This is the
  // proven tablet preset (all days): seat heating OFF 00:00-06:00, 09:00-19:00, 22:00-23:59; warm in
  // the 06:00-09:00 and 19:00-22:00 gaps. ecoSeatTemp = 0. Frame is 22 bytes (split by write_raw_).
  std::vector<uint8_t> payload;
  if (state) {
    payload.push_back(0x01);  // (ecoSeatTemp<<4) | on
    payload.push_back(0x03);  // schedule count
    static const uint8_t windows[3][5] = {
        {0x7F, 0, 0, 6, 0},     // all days, 00:00 -> 06:00
        {0x7F, 9, 0, 19, 0},    // all days, 09:00 -> 19:00
        {0x7F, 22, 0, 23, 59},  // all days, 22:00 -> 23:59
    };
    for (auto &w : windows)
      for (uint8_t byte : w)
        payload.push_back(byte);
  } else {
    payload.push_back(0x00);  // off: single state byte
  }
  this->enqueue_command(OP_ENERGY_SAVING, payload);
  // byte3 of the 0x51 readback is a schedule count; track optimistically so func-config parsing
  // never overwrites the switch.
  this->energy_saving_optimistic_ = state;
  if (this->energy_saving_switch_ != nullptr)
    this->energy_saving_switch_->publish_state(state);
}

// ---- washing ----
// rear/lady wash payloads pack all four params (verified against the app's SerialPacketFactory):
// byte0 = (waterFlow<<4)|nozzlePosition, byte1 = (comfortWash<<4)|waterTemperature.
void SensowashSerial::set_wash_param(uint8_t kind, uint8_t index) {
  // No-op if unchanged, so re-asserting the current value (e.g. a second open dashboard) sends nothing.
  switch (kind) {
    case 1:
      if (index == this->water_flow_index_) return;
      this->water_flow_index_ = index; this->enqueue_command(OP_WATER_FLOW, {index}); break;
    case 2:
      if (index == this->nozzle_position_index_) return;
      this->nozzle_position_index_ = index; this->enqueue_command(OP_NOZZLE_POSITION, {index}); break;
    case 3:
      if (index == this->wash_water_temp_index_) return;
      this->wash_water_temp_index_ = index; this->enqueue_command(OP_WATER_TEMP, {index}); break;
    default: return;
  }
  this->save_wash_params_();
  this->pending_poll_ms_ = millis() + POST_CMD_POLL_DELAY_MS;
}

void SensowashSerial::set_comfort_wash(bool state) {
  if (state == this->comfort_wash_)
    return;  // no-op if unchanged
  this->comfort_wash_ = state;
  this->enqueue_command(OP_COMFORT_WASH, {(uint8_t) (state ? 1 : 0)});
  this->pending_poll_ms_ = millis() + POST_CMD_POLL_DELAY_MS;
}

void SensowashSerial::start_wash(uint8_t kind) {
  uint8_t b0 = (uint8_t) ((this->water_flow_index_ << 4) | (this->nozzle_position_index_ & 0x0F));
  uint8_t b1 = (uint8_t) (((this->comfort_wash_ ? 1 : 0) << 4) | (this->wash_water_temp_index_ & 0x0F));
  this->enqueue_command(kind == 2 ? OP_LADY_WASH : OP_REAR_WASH, {b0, b1});
  this->pending_poll_ms_ = millis() + POST_CMD_POLL_DELAY_MS;
}

void SensowashSerial::set_holiday_mode(bool state) {
  if (state) {
    // 0x62 [01] = enter holiday mode / drain the internal tank (EN1717). The toilet never reports
    // this state back, so we track it optimistically and clear it on the next seat use.
    this->enqueue_command(0x62, {0x01});
    this->holiday_active_ = true;
    ESP_LOGI(TAG, "Holiday mode ON (tank drain requested)");
  } else {
    this->holiday_active_ = false;
    ESP_LOGI(TAG, "Holiday mode OFF");
  }
  if (this->holiday_switch_ != nullptr)
    this->holiday_switch_->publish_state(state);
}

// ---------------------------------------------------------------------------
// BLE plumbing
// ---------------------------------------------------------------------------

bool SensowashSerial::resolve_handles_() {
  auto svc = espbt::ESPBTUUID::from_raw(SERVICE_UUID);
  auto *wr = this->parent()->get_characteristic(svc, espbt::ESPBTUUID::from_raw(WRITE_UUID));
  auto *nt = this->parent()->get_characteristic(svc, espbt::ESPBTUUID::from_raw(NOTIFY_UUID));
  auto *hs = this->parent()->get_characteristic(svc, espbt::ESPBTUUID::from_raw(HANDSHAKE_UUID));
  if (wr == nullptr || nt == nullptr || hs == nullptr) {
    ESP_LOGE(TAG, "Required SensoWash characteristics not found (write=%p notify=%p handshake=%p)", wr, nt, hs);
    return false;
  }
  this->write_handle_ = wr->handle;
  this->notify_handle_ = nt->handle;
  this->handshake_handle_ = hs->handle;
  ESP_LOGD(TAG, "Resolved handles write=0x%04X notify=0x%04X handshake=0x%04X", this->write_handle_,
           this->notify_handle_, this->handshake_handle_);
  return true;
}

void SensowashSerial::enable_notify_(uint16_t char_handle) {
  // The toilet's GATT descriptors don't enumerate on the ESP (esp_ble_gattc_get_all_descr returns
  // NOT_FOUND), so neither ESPHome's cache nor the IDF lookups can find the CCCD. Resolve it the
  // robust way: prefer the discovered descriptor if present, otherwise write the enable value to
  // the conventional CCCD location (characteristic value handle + 1), which is where the Android
  // app wrote it. Without this, no notifications arrive and the toilet stays silent.
  uint16_t cccd_handle = 0;
  auto *descr = this->parent()->get_config_descriptor(char_handle);
  if (descr != nullptr) {
    cccd_handle = descr->handle;
  } else {
    cccd_handle = char_handle + 1;
    ESP_LOGW(TAG, "CCCD not discoverable for char 0x%04X; writing value+1 (0x%04X) directly", char_handle,
             cccd_handle);
  }
  uint16_t enable = 1;
  auto status =
      esp_ble_gattc_write_char_descr(this->parent()->get_gattc_if(), this->parent()->get_conn_id(), cccd_handle,
                                     sizeof(enable), (uint8_t *) &enable, ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
  ESP_LOGI(TAG, "Enable-notify char=0x%04X cccd=0x%04X status=%d", char_handle, cccd_handle, status);
}

void SensowashSerial::write_raw_(uint16_t handle, const std::vector<uint8_t> &data) {
  if (handle == 0)
    return;
  // MTU is 23 (max 20-byte ATT payload). Frames >20 bytes (e.g. the energy-saving schedule) must be
  // split into 20-byte writes, in order, for the toilet's serial bridge to reassemble them.
  const size_t MAX = 20;
  for (size_t off = 0; off < data.size(); off += MAX) {
    size_t n = std::min(MAX, data.size() - off);
    esp_ble_gattc_write_char(this->parent()->get_gattc_if(), this->parent()->get_conn_id(), handle, n,
                             (uint8_t *) (data.data() + off), ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);
  }
}

void SensowashSerial::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                          esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_CONNECT_EVT: {
      ESP_LOGI(TAG, "GATTC CONNECT_EVT conn_id=%d", param->connect.conn_id);
      break;
    }

    case ESP_GATTC_OPEN_EVT: {
      ESP_LOGI(TAG, "GATTC OPEN_EVT status=%d conn_id=%d mtu=%d", param->open.status, param->open.conn_id,
               param->open.mtu);
      break;
    }

    case ESP_GATTC_DISCONNECT_EVT: {
      ESP_LOGW(TAG, "GATTC DISCONNECT_EVT reason=0x%02X conn_id=%d", param->disconnect.reason,
               param->disconnect.conn_id);
      this->reset_connection_();
      break;
    }
    case ESP_GATTC_CLOSE_EVT: {
      ESP_LOGW(TAG, "GATTC CLOSE_EVT reason=0x%02X conn_id=%d", param->close.reason, param->close.conn_id);
      this->reset_connection_();
      break;
    }

    case ESP_GATTC_SEARCH_CMPL_EVT: {
      ESP_LOGI(TAG, "GATTC SEARCH_CMPL_EVT (service discovery done)");
      if (!this->resolve_handles_()) {
        break;
      }
      // The BLE connection itself is up; protocol handshake is tracked separately.
      this->node_state = espbt::ClientState::ESTABLISHED;
      this->start_handshake_();
      break;
    }

    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
      uint16_t h = param->reg_for_notify.handle;
      this->enable_notify_(h);
      if (h == this->handshake_handle_) {
        // Now safe to issue the challenge / known-key write.
        if (this->key_known_) {
          ESP_LOGI(TAG, "Writing known handshake key (skipping enrollment wait)");
          this->write_raw_(this->handshake_handle_, {this->key_[0], this->key_[1], this->key_[2], this->key_[3]});
        } else {
          ESP_LOGI(TAG, "No key known: writing 00000000 challenge and awaiting enrollment");
          this->write_raw_(this->handshake_handle_, {0x00, 0x00, 0x00, 0x00});
          this->handshake_state_ = HandshakeState::AWAITING_KEY;
          this->awaiting_key_since_ = millis();
          this->awaiting_key_logged_ = false;
          this->publish_pairing_("awaiting_key");
        }
        // Subscribe the main notify channel next.
        auto st = esp_ble_gattc_register_for_notify(this->parent()->get_gattc_if(), this->parent()->get_remote_bda(),
                                                    this->notify_handle_);
        ESP_LOGI(TAG, "register_for_notify(notify 0x%04X) -> %d", this->notify_handle_, st);
      } else if (h == this->notify_handle_) {
        this->notify_subscribed_ = true;
        this->maybe_finish_();
      }
      break;
    }

    case ESP_GATTC_NOTIFY_EVT: {
      if (param->notify.handle == this->handshake_handle_) {
        if (this->handshake_state_ == HandshakeState::AWAITING_KEY && param->notify.value_len >= 4) {
          ESP_LOGI(TAG, "Learned handshake key %02x%02x%02x%02x", param->notify.value[0], param->notify.value[1],
                   param->notify.value[2], param->notify.value[3]);
          this->store_key_(param->notify.value);
          this->maybe_finish_();
        }
      } else if (param->notify.handle == this->notify_handle_) {
        this->handle_notify_(param->notify.value, param->notify.value_len);
      }
      break;
    }

    default:
      break;
  }
}

// ---------------------------------------------------------------------------
// Handshake state machine
// ---------------------------------------------------------------------------

void SensowashSerial::start_handshake_() {
  this->handshake_state_ = HandshakeState::IDLE;
  this->notify_subscribed_ = false;
  this->publish_pairing_("connecting");
  // Subscribe the handshake channel first; the rest is driven from REG_FOR_NOTIFY / NOTIFY events.
  auto st = esp_ble_gattc_register_for_notify(this->parent()->get_gattc_if(), this->parent()->get_remote_bda(),
                                              this->handshake_handle_);
  ESP_LOGI(TAG, "register_for_notify(handshake 0x%04X) -> %d", this->handshake_handle_, st);
}

void SensowashSerial::store_key_(const uint8_t *key) {
  std::memcpy(this->key_, key, 4);
  this->key_known_ = true;
  std::array<uint8_t, 4> arr;
  std::memcpy(arr.data(), key, 4);
  if (this->pref_.save(&arr)) {
    global_preferences->sync();
    ESP_LOGI(TAG, "Persisted handshake key");
  }
}

void SensowashSerial::maybe_finish_() {
  if (this->handshake_state_ == HandshakeState::READY)
    return;
  if (this->key_known_ && this->notify_subscribed_)
    this->finish_handshake_();
}

void SensowashSerial::finish_handshake_() {
  this->handshake_state_ = HandshakeState::READY;
  this->last_tx_ms_ = millis();
  this->last_seat_poll_ms_ = millis();  // initial polls already request toilet-state; defer periodic poll
  this->last_rx_ms_ = millis();         // watchdog baseline
  ESP_LOGI(TAG, "Handshake complete; ready");
  this->publish_pairing_("paired");
  this->publish_wash_params_();  // reflect persisted wash params (the toilet doesn't report them)
  this->send_initial_polls_();
  for (auto *t : this->on_connect_triggers_)
    t->trigger();
}

void SensowashSerial::send_initial_polls_() {
  // Queued and paced by loop(); order follows the captured app behaviour.
  this->enqueue_command(OP_REQ_SW_VERSION);
  this->enqueue_command(OP_REQ_HW_VERSION);
  this->enqueue_command(OP_REQ_SERIAL);
  this->enqueue_command(OP_REQ_FUNC_CONFIG);
  this->enqueue_command(OP_REQ_TOILET_STATE);
  this->enqueue_command(OP_REQ_ERROR_CODES);
  this->enqueue_command(OP_REQ_DESCALING_STATE);
  this->enqueue_command(OP_REQ_DESCALING_REMAINING);
}

void SensowashSerial::reset_connection_() {
  bool was_connected = this->handshake_state_ != HandshakeState::IDLE || this->write_handle_ != 0;
  this->handshake_state_ = HandshakeState::IDLE;
  this->write_handle_ = 0;
  this->notify_handle_ = 0;
  this->handshake_handle_ = 0;
  this->notify_subscribed_ = false;
  this->awaiting_key_logged_ = false;
  this->energy_saving_seeded_ = false;
  this->tx_queue_.clear();
  this->rx_buffer_.clear();
  this->pending_poll_ms_ = 0;
  this->fault_active_ = false;
  this->descale_needed_ = false;
  // Clear the toilet BLE signal so HA doesn't keep showing a stale value while disconnected.
  if (this->ble_rssi_sensor_ != nullptr)
    this->ble_rssi_sensor_->publish_state(NAN);
  if (was_connected) {
    ESP_LOGI(TAG, "Disconnected");
    for (auto *t : this->on_disconnect_triggers_)
      t->trigger();
  }
  this->publish_pairing_("offline");
}

// ---------------------------------------------------------------------------
// Notify / response handling
// ---------------------------------------------------------------------------

void SensowashSerial::handle_notify_(const uint8_t *data, uint16_t len) {
  this->last_rx_ms_ = millis();  // liveness watchdog: any inbound data proves the link is alive
  // Append this notification chunk and extract every complete frame. Responses larger than the
  // 20-byte ATT payload (e.g. func-config with schedules) arrive split across notifications, so a
  // single parse of one chunk would reject them as "bad". Reassemble by the length header here.
  this->rx_buffer_.insert(this->rx_buffer_.end(), data, data + len);

  while (this->rx_buffer_.size() >= 5) {
    // Resync on the 55 05 sync word; drop any leading garbage.
    size_t start = 0;
    while (start + 1 < this->rx_buffer_.size() &&
           !(this->rx_buffer_[start] == 0x55 && this->rx_buffer_[start + 1] == 0x05))
      start++;
    if (start > 0)
      this->rx_buffer_.erase(this->rx_buffer_.begin(), this->rx_buffer_.begin() + start);
    if (this->rx_buffer_.size() < 5)
      break;

    uint8_t flen = this->rx_buffer_[2];
    size_t total = (size_t) flen + 2;  // total frame bytes = len + 2
    if (this->rx_buffer_.size() < total)
      break;  // incomplete; wait for the next notification

    uint8_t opcode;
    std::vector<uint8_t> payload;
    if (this->parse_frame_(this->rx_buffer_.data(), total, opcode, payload)) {
      ESP_LOGI(TAG, "RX %s",
               format_hex_pretty(std::vector<uint8_t>(this->rx_buffer_.begin(),
                                                      this->rx_buffer_.begin() + total))
                   .c_str());
      this->dispatch_opcode_(opcode, payload);
      this->rx_buffer_.erase(this->rx_buffer_.begin(), this->rx_buffer_.begin() + total);
    } else {
      // Bad checksum / framing: drop the sync word and resync past it.
      ESP_LOGW(TAG, "Bad frame (len=%u), resyncing", (unsigned) flen);
      this->rx_buffer_.erase(this->rx_buffer_.begin(), this->rx_buffer_.begin() + 2);
    }
  }

  // Safety valve against an un-resyncable buffer growing unbounded.
  if (this->rx_buffer_.size() > 128)
    this->rx_buffer_.clear();
}

void SensowashSerial::dispatch_opcode_(uint8_t opcode, const std::vector<uint8_t> &payload) {
  switch (opcode) {
    case OP_TIME_REQUEST:
      this->handle_time_request_();
      break;
    case OP_RESP_FUNC_CONFIG:
      this->parse_func_config_(payload);
      break;
    case OP_RESP_TOILET_STATE:
      this->parse_toilet_state_(payload);
      break;
    case OP_RESP_ERROR_CODES:
      this->parse_error_codes_(payload);
      break;
    case OP_RESP_SW_VERSION:
      this->publish_text_(this->software_version_tsensor_, payload);
      break;
    case OP_RESP_SERIAL:
      // The serial response is BCD/hex digits (e.g. 12 34 56 78 90 12 = "123456789012"), not ASCII.
      if (this->serial_number_tsensor_ != nullptr) {
        std::string s;
        char b[3];
        for (auto byte : payload) {
          snprintf(b, sizeof(b), "%02X", byte);
          s += b;
        }
        this->serial_number_tsensor_->publish_state(s);
      }
      break;
    case OP_RESP_HW_VERSION:
      this->publish_text_(this->hardware_version_tsensor_, payload);
      break;
    case OP_RESP_DESCALING_STATE:
      this->parse_descaling_state_(payload);
      break;
    case OP_RESP_DESCALING_REMAINING:
      this->parse_descaling_remaining_(payload);
      break;
    default:
      ESP_LOGD(TAG, "Unhandled response opcode 0x%02X", opcode);
      break;
  }
}

void SensowashSerial::handle_time_request_() {
  std::time_t now = ::time(nullptr);
  std::tm tm_buf{};
#ifdef _WIN32
  localtime_s(&tm_buf, &now);
  std::tm *t = &tm_buf;
#else
  std::tm *t = ::localtime(&now);
#endif
  std::vector<uint8_t> payload;
  if (t != nullptr && (t->tm_year + 1900) >= 2021) {
    payload = {(uint8_t) ((t->tm_year + 1900 - 2000) & 0xFF),
               (uint8_t) (t->tm_mon + 1),
               (uint8_t) t->tm_mday,
               (uint8_t) t->tm_hour,
               (uint8_t) t->tm_min,
               (uint8_t) t->tm_sec};
  } else {
    // No synced clock yet: send a benign default so the toilet proceeds.
    payload = {25, 1, 1, 12, 0, 0};
  }
  ESP_LOGD(TAG, "Replying to time request (0x2A) with 0x2B");
  this->enqueue_command(OP_TIME_REPLY, payload);
}

void SensowashSerial::parse_func_config_(const std::vector<uint8_t> &p) {
  if (p.size() < 3) {
    ESP_LOGW(TAG, "Short func-config payload (%u)", (unsigned) p.size());
    return;
  }
  uint8_t b0 = p[0];
  uint8_t b1 = p[1];
  uint8_t b2 = p[2];
  uint8_t b3 = p.size() > 3 ? p[3] : 0;

  // Bit layout verified against the decompiled app (FunctionConfiguration constructor + parse):
  //  byte0: bit0=beep, bits2-3=night-light, bit6=auto-lid-open, bit7=auto-lid-close
  //  byte1: bit0/1=flush/preflush (n/a here), bits2-3=comfort-wash (OnOffState; 1=on), bit4=auto-deo
  //  byte2: bits4-5=seat-temp ; byte3: energy-saving schedule count
  bool beep = bits(b0, 0, 0);
  uint8_t night_light = bits(b0, 2, 3);  // 0=off 1=on 2=auto
  bool auto_open = bits(b0, 6, 6);
  bool auto_close = bits(b0, 7, 7);
  bool comfort = bits(b1, 2, 3) == 1;
  uint8_t seat_temp = bits(b2, 4, 5);

  if (this->beep_switch_ != nullptr)
    this->beep_switch_->publish_state(beep);
  if (this->auto_lid_open_switch_ != nullptr)
    this->auto_lid_open_switch_->publish_state(auto_open);
  if (this->auto_lid_close_switch_ != nullptr)
    this->auto_lid_close_switch_->publish_state(auto_close);
  this->comfort_wash_ = comfort;
  if (this->comfort_wash_switch_ != nullptr)
    this->comfort_wash_switch_->publish_state(comfort);

  this->seat_temp_index_ = seat_temp;
  if (this->seat_temperature_select_ != nullptr) {
    auto opt = this->seat_temperature_select_->at(seat_temp);
    if (opt.has_value())
      this->seat_temperature_select_->publish_state(*opt);
  }
  if (this->night_light_select_ != nullptr) {
    auto opt = this->night_light_select_->at(night_light);
    if (opt.has_value())
      this->night_light_select_->publish_state(*opt);
  }

  // Energy saving: seed once from byte3 schedule count, then leave it to optimistic tracking.
  if (!this->energy_saving_seeded_) {
    this->energy_saving_optimistic_ = b3 > 0;
    if (this->energy_saving_switch_ != nullptr)
      this->energy_saving_switch_->publish_state(this->energy_saving_optimistic_);
    this->energy_saving_seeded_ = true;
  }
}

void SensowashSerial::parse_toilet_state_(const std::vector<uint8_t> &p) {
  if (p.empty())
    return;
  uint8_t b0 = p[0];
  bool occupied = bits(b0, 2, 2);
  bool drying = bits(b0, 4, 4);

  if (this->seat_occupied_bsensor_ != nullptr)
    this->seat_occupied_bsensor_->publish_state(occupied);

  // Holiday mode auto-exits when the toilet is next used: a rising edge on seat occupancy clears it.
  if (occupied && !this->last_seat_occupied_ && this->holiday_active_) {
    ESP_LOGI(TAG, "Seat used -> exiting Holiday mode");
    this->holiday_active_ = false;
    if (this->holiday_switch_ != nullptr)
      this->holiday_switch_->publish_state(false);
  }
  this->last_seat_occupied_ = occupied;

  if (this->dryer_select_ != nullptr) {
    uint8_t idx;
    if (drying) {
      idx = this->last_dryer_temp_index_ > 0 ? this->last_dryer_temp_index_ : 1;  // show something sensible
    } else {
      idx = 0;
    }
    auto opt = this->dryer_select_->at(idx);
    if (opt.has_value())
      this->dryer_select_->publish_state(*opt);
  }
}

void SensowashSerial::parse_error_codes_(const std::vector<uint8_t> &p) {
  // Bitmask: payload byte i, bit b set = error number (i*8 + b + 1). Publish the active numbers as
  // CSV ("none" if clear); the dashboard maps each number to the service code + description.
  std::string csv;
  for (size_t i = 0; i < p.size(); i++) {
    uint8_t b = p[i];
    for (int bit = 0; bit < 8; bit++) {
      if (b & (1 << bit)) {
        if (!csv.empty())
          csv += ",";
        csv += std::to_string((int) (i * 8 + bit + 1));
      }
    }
  }
  this->fault_active_ = !csv.empty();  // drives the red/blue fault pattern on the status LED
  if (this->fault_codes_tsensor_ != nullptr)
    this->fault_codes_tsensor_->publish_state(csv.empty() ? "none" : csv);
}

void SensowashSerial::parse_descaling_state_(const std::vector<uint8_t> &p) {
  if (p.empty())
    return;
  this->descale_needed_ = (p[0] == 1);  // "Descaling required" -> amber status LED
  if (this->descaling_state_tsensor_ == nullptr)
    return;
  const char *txt;
  switch (p[0]) {
    case 0: txt = "Not required"; break;
    case 1: txt = "Descaling required"; break;
    case 2: txt = "Descaling running"; break;
    case 3: txt = "Descaling complete"; break;
    default: txt = "Unknown"; break;
  }
  this->descaling_state_tsensor_->publish_state(txt);
}

void SensowashSerial::parse_descaling_remaining_(const std::vector<uint8_t> &p) {
  if (this->descaling_remaining_sensor_ == nullptr || p.size() < 2)
    return;
  uint16_t days = (uint16_t) (p[0] << 8) | p[1];
  this->descaling_remaining_sensor_->publish_state(days);
}

void SensowashSerial::publish_text_(text_sensor::TextSensor *s, const std::vector<uint8_t> &p) {
  if (s == nullptr)
    return;
  std::string str(p.begin(), p.end());
  // Trim trailing NULs/whitespace.
  while (!str.empty() && (str.back() == '\0' || str.back() == ' ' || str.back() == '\r' || str.back() == '\n'))
    str.pop_back();
  s->publish_state(str);
}

// Pairing/handshake stage for the dashboard banner: offline / connecting / awaiting_key / paired.
void SensowashSerial::publish_pairing_(const char *state) {
  if (this->pairing_status_tsensor_ != nullptr)
    this->pairing_status_tsensor_->publish_state(state);
}

void SensowashSerial::save_wash_params_() {
  std::array<uint8_t, 3> wp{this->water_flow_index_, this->nozzle_position_index_, this->wash_water_temp_index_};
  if (this->wash_pref_.save(&wp))
    global_preferences->sync();
}

void SensowashSerial::publish_wash_params_() {
  if (this->water_flow_select_ != nullptr) {
    auto o = this->water_flow_select_->at(this->water_flow_index_);
    if (o.has_value())
      this->water_flow_select_->publish_state(*o);
  }
  if (this->nozzle_position_select_ != nullptr) {
    auto o = this->nozzle_position_select_->at(this->nozzle_position_index_);
    if (o.has_value())
      this->nozzle_position_select_->publish_state(*o);
  }
  if (this->wash_water_temp_select_ != nullptr) {
    auto o = this->wash_water_temp_select_->at(this->wash_water_temp_index_);
    if (o.has_value())
      this->wash_water_temp_select_->publish_state(*o);
  }
}

// Onboard RGB status LED: off when not connected; solid blue when connected; brief blink-off on each
// TX (so it "flashes" while transmitting); alternating red/blue when the toilet reports a fault.
void SensowashSerial::update_led_() {
  if (this->status_led_ == nullptr)
    return;
  bool on = false;
  uint8_t r = 0, g = 0, b = 0;
  float bright = 1.0f;
  if (this->handshake_state_ == HandshakeState::READY) {
    if (this->fault_active_) {
      if ((millis() / LED_FAULT_BLINK_MS) % 2 == 0) { on = true; r = 255; }  // red
      else { on = true; b = 255; }                                          // blue
    } else if ((int32_t) (millis() - this->led_flash_until_ms_) < 0) {
      on = false;  // transmitting: brief blink-off
    } else if (this->descale_needed_) {
      on = true; r = 255; g = 140; b = 0;  // descaling required: amber
    } else {
      on = true; b = 255; bright = 0.2f;  // connected, idle: blue at 20% brightness
    }
  }
  if (on == this->led_on_ && r == this->led_r_ && g == this->led_g_ && b == this->led_b_ &&
      bright == this->led_bright_)
    return;  // unchanged — don't re-write the LED every loop
  this->led_on_ = on; this->led_r_ = r; this->led_g_ = g; this->led_b_ = b; this->led_bright_ = bright;
  auto call = this->status_led_->make_call();
  call.set_transition_length(0);
  if (on) {
    call.set_state(true);
    call.set_brightness(bright);
    call.set_rgb(r / 255.0f, g / 255.0f, b / 255.0f);
  } else {
    call.set_state(false);
  }
  call.perform();
}

}  // namespace sensowash_serial
}  // namespace esphome

#endif  // USE_ESP32
