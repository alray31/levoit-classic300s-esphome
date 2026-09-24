#pragma once

// ESPHome external component for the Levoit Classic 300S humidifier.
//
// Replaces the stock firmware on the built-in WiFi/ESP module (ESP32-SOLO-1C
// or the newer ESP32-C3-SOLO-1) and speaks the appliance MCU's proprietary
// "A5" UART protocol directly.
//
// Protocol reverse-engineered by Maxim Pivovarov / MaxPi ("Taxom"), via live
// UART sniffing and active replacement-controller testing:
//   https://github.com/Taxom/levoit-classic-300s-uart-protocol
//   (CC BY-NC-SA 4.0 -- protocol facts used here, component code is original)
//
// This file implements the hub: frame parsing/building, checksum, status
// decode, and the read-only entities (sensor/binary_sensor/text_sensor).
// Controllable entities (switch/number/select) live in their own platform
// files and talk to this hub through the setters/actions below.

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"

#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace esphome {
namespace lv_classic300s_humidifier {

enum WorkMode : uint8_t {
  MODE_AUTO = 0x00,
  MODE_MANUAL = 0x01,
  MODE_SLEEP = 0x02,
  MODE_UNKNOWN = 0xFF,
};

// Frame types seen on the A5 bus.
enum A5FrameType : uint8_t {
  A5_TYPE_CMD = 0x22,    // WiFi/ESP -> MCU command
  A5_TYPE_ACK = 0x12,    // MCU -> WiFi/ESP reply / ACK / status reply
  A5_TYPE_NACK = 0x52,   // MCU -> WiFi/ESP NACK / rejected command
  A5_TYPE_STATUS = 0x02, // MCU -> WiFi/ESP full status or short event
};

class LVClassic300SHumidifier : public Component, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_status_interval(uint32_t interval_ms) { this->status_interval_ms_ = interval_ms; }

  // --- read-only entities (set from YAML via nested schema) ---
  void set_current_humidity_sensor(sensor::Sensor *s) { this->current_humidity_sensor_ = s; }
  void set_target_humidity_sensor(sensor::Sensor *s) { this->target_humidity_sensor_ = s; }
  void set_temperature_sensor(sensor::Sensor *s) { this->temperature_sensor_ = s; }
  void set_night_light_sensor(sensor::Sensor *s) { this->night_light_sensor_ = s; }
  void set_output_level_sensor(sensor::Sensor *s) { this->output_level_sensor_ = s; }

  void set_power_binary_sensor(binary_sensor::BinarySensor *s) { this->power_binary_sensor_ = s; }
  void set_tank_removed_sensor(binary_sensor::BinarySensor *s) { this->tank_removed_sensor_ = s; }
  void set_water_empty_sensor(binary_sensor::BinarySensor *s) { this->water_empty_sensor_ = s; }
  void set_mist_active_sensor(binary_sensor::BinarySensor *s) { this->mist_active_sensor_ = s; }
  void set_display_binary_sensor(binary_sensor::BinarySensor *s) { this->display_binary_sensor_ = s; }
  void set_target_stop_active_sensor(binary_sensor::BinarySensor *s) { this->target_stop_active_sensor_ = s; }

  void set_mode_text_sensor(text_sensor::TextSensor *s) { this->mode_text_sensor_ = s; }
  void set_error_text_sensor(text_sensor::TextSensor *s) { this->error_text_sensor_ = s; }
  void set_last_status_frame_text_sensor(text_sensor::TextSensor *s) { this->last_status_frame_text_sensor_ = s; }

  // --- commands, used by the switch/number/select platforms ---
  void request_status();
  void set_power(bool on);
  void set_display(bool on);
  void set_night_light(uint8_t percent);          // 0..100
  void set_stop_at_target(bool on);
  void send_mode_sync_preamble();
  void set_manual_level(uint8_t level);            // 1..9, call send_mode_sync_preamble() first
  void set_auto_target(uint8_t target_percent);    // call send_mode_sync_preamble() first
  void set_sleep_target(uint8_t target_percent);   // call send_mode_sync_preamble() first
  void set_mode(WorkMode mode, uint8_t target_or_level);

  // current decoded state, exposed so switch/number/select platforms can
  // report a sane initial/optimistic value without waiting on a round-trip
  bool power_state() const { return this->power_state_; }
  bool display_state() const { return this->display_state_; }
  bool stop_at_target_state() const { return this->stop_at_target_state_; }
  uint8_t night_light_level() const { return this->night_light_level_; }
  uint8_t manual_level() const { return this->manual_level_; }
  uint8_t auto_target() const { return this->auto_target_; }
  uint8_t sleep_target() const { return this->sleep_target_; }
  WorkMode work_mode() const { return this->work_mode_; }
  // true once at least one real 20-byte status frame has been parsed from
  // the appliance. Used by common_entities.yaml's apply_night_light_state
  // script to avoid making a Night Light decision from default/stale
  // binary_sensor values (all false) in the brief window between boot and
  // the first real status reply -- see that script for why.
  bool got_first_status() const { return this->got_first_status_; }

  // fired whenever a new decoded status is available, used by the select/
  // number/switch platforms to keep their reported state in sync with what
  // the MCU actually reports (including physical front-panel changes)
  void add_on_status_callback(std::function<void()> &&cb) { this->status_callback_.add(std::move(cb)); }

  // the MCU reports physical power-button long-presses as short A5-02
  // events (see docs/A5_UART_protocol.md). Stock firmware uses ~5s hold for
  // WiFi pairing/reconnect and ~15s hold for factory reset; wire these up in
  // YAML (e.g. to esphome.restart / a captive portal / a status light)
  // instead of hardcoding a specific recovery behavior here.
  Trigger<> *get_power_hold_5s_trigger() { return &this->power_hold_5s_trigger_; }
  Trigger<> *get_power_hold_15s_trigger() { return &this->power_hold_15s_trigger_; }

 protected:
  void handle_incoming_byte_(uint8_t byte);
  void handle_frame_(const std::vector<uint8_t> &frame);
  void handle_status_payload_(const uint8_t *p, size_t len);
  void handle_short_event_(const uint8_t *p, size_t len);
  void send_command_(const uint8_t *payload, size_t len);

  // receive buffer / simple state machine
  std::vector<uint8_t> rx_buffer_;
  uint32_t last_rx_byte_ms_{0};

  uint8_t next_packet_id_{1};
  uint32_t status_interval_ms_{15000};
  uint32_t last_status_request_ms_{0};
  bool got_first_status_{false};

  // decoded state mirror
  bool power_state_{false};
  bool tank_removed_{false};
  bool water_empty_{false};
  bool target_stop_active_{false};
  bool display_state_{false};
  bool mist_active_{false};
  bool stop_at_target_state_{false};
  uint8_t target_humidity_{0};
  uint8_t current_humidity_{0};
  int8_t temperature_c_{0};
  WorkMode work_mode_{MODE_UNKNOWN};
  uint8_t output_level_{0};
  uint8_t manual_level_{5};
  uint8_t auto_target_{45};
  uint8_t sleep_target_{45};
  uint8_t night_light_level_{0};
  uint8_t error_code_{0};

  sensor::Sensor *current_humidity_sensor_{nullptr};
  sensor::Sensor *target_humidity_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *night_light_sensor_{nullptr};
  sensor::Sensor *output_level_sensor_{nullptr};

  binary_sensor::BinarySensor *power_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *tank_removed_sensor_{nullptr};
  binary_sensor::BinarySensor *water_empty_sensor_{nullptr};
  binary_sensor::BinarySensor *mist_active_sensor_{nullptr};
  binary_sensor::BinarySensor *display_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *target_stop_active_sensor_{nullptr};

  text_sensor::TextSensor *mode_text_sensor_{nullptr};
  text_sensor::TextSensor *error_text_sensor_{nullptr};
  text_sensor::TextSensor *last_status_frame_text_sensor_{nullptr};

  CallbackManager<void()> status_callback_;

  Trigger<> power_hold_5s_trigger_;
  Trigger<> power_hold_15s_trigger_;
};

}  // namespace lv_classic300s_humidifier
}  // namespace esphome
