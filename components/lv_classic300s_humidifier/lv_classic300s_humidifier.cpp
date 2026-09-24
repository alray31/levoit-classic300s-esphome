#include "lv_classic300s_humidifier.h"
#include "esphome/core/log.h"
#include <cstdio>
#include <cstring>

namespace esphome {
namespace lv_classic300s_humidifier {

static const char *const TAG = "lv_classic300s_humidifier";

static const uint32_t RX_TIMEOUT_MS = 250;
static const size_t RX_BUFFER_MAX = 128;

static uint8_t a5_checksum(const uint8_t *frame, size_t total_len) {
  uint16_t sum = 0;
  for (size_t i = 0; i < total_len; i++) {
    if (i != 5) sum += frame[i];
  }
  return static_cast<uint8_t>(0xFF - (sum & 0xFF));
}

static std::string frame_to_hex(const uint8_t *data, size_t len) {
  std::string out;
  out.reserve(len * 3);
  char buf[4];
  for (size_t i = 0; i < len; i++) {
    snprintf(buf, sizeof(buf), "%02X ", data[i]);
    out += buf;
  }
  if (!out.empty()) out.pop_back();
  return out;
}

void LVClassic300SHumidifier::setup() {
  this->rx_buffer_.reserve(32);
  // recommended init sequence: hide timer icon, then request full status
  const uint8_t timer_icon_off[] = {0x01, 0x6A, 0xA2, 0x00, 0x00};
  this->send_command_(timer_icon_off, sizeof(timer_icon_off));
  this->request_status();
  this->last_status_request_ms_ = millis();
}

void LVClassic300SHumidifier::loop() {
  while (this->available()) {
    uint8_t byte;
    if (this->read_byte(&byte)) {
      this->handle_incoming_byte_(byte);
    }
  }

  const uint32_t now = millis();

  // drop a stale partial frame instead of waiting forever for the rest of it
  if (!this->rx_buffer_.empty() && (now - this->last_rx_byte_ms_) > RX_TIMEOUT_MS) {
    ESP_LOGV(TAG, "Dropping stale partial frame (%u bytes)", this->rx_buffer_.size());
    this->rx_buffer_.clear();
  }

  if (now - this->last_status_request_ms_ >= this->status_interval_ms_) {
    this->request_status();
    this->last_status_request_ms_ = now;
  }
}

void LVClassic300SHumidifier::dump_config() {
  ESP_LOGCONFIG(TAG, "Levoit Classic 300S humidifier:");
  ESP_LOGCONFIG(TAG, "  Status poll interval: %lums", this->status_interval_ms_);
}

void LVClassic300SHumidifier::handle_incoming_byte_(uint8_t byte) {
  this->last_rx_byte_ms_ = millis();
  this->rx_buffer_.push_back(byte);

  if (this->rx_buffer_.size() > RX_BUFFER_MAX) {
    ESP_LOGW(TAG, "RX buffer overflow, clearing");
    this->rx_buffer_.clear();
    return;
  }

  // resync on the 0xA5 magic byte
  while (!this->rx_buffer_.empty() && this->rx_buffer_[0] != 0xA5) {
    this->rx_buffer_.erase(this->rx_buffer_.begin());
  }

  if (this->rx_buffer_.size() < 5) return;  // not enough to know the payload length yet

  const uint16_t payload_len = this->rx_buffer_[3] | (static_cast<uint16_t>(this->rx_buffer_[4]) << 8);
  const size_t total_len = static_cast<size_t>(payload_len) + 6;

  if (total_len > RX_BUFFER_MAX) {
    // implausible length, this 0xA5 was noise -- drop it and resync on the next one
    ESP_LOGV(TAG, "Implausible frame length %u, resyncing", (unsigned) total_len);
    this->rx_buffer_.erase(this->rx_buffer_.begin());
    return;
  }

  if (this->rx_buffer_.size() < total_len) return;  // wait for the rest of the frame

  if (a5_checksum(this->rx_buffer_.data(), total_len) != this->rx_buffer_[5]) {
    ESP_LOGW(TAG, "Bad checksum, dropping byte and resyncing: %s",
             frame_to_hex(this->rx_buffer_.data(), total_len).c_str());
    this->rx_buffer_.erase(this->rx_buffer_.begin());
    return;
  }

  std::vector<uint8_t> frame(this->rx_buffer_.begin(), this->rx_buffer_.begin() + total_len);
  this->rx_buffer_.erase(this->rx_buffer_.begin(), this->rx_buffer_.begin() + total_len);
  this->handle_frame_(frame);
}

void LVClassic300SHumidifier::handle_frame_(const std::vector<uint8_t> &frame) {
  const uint8_t type = frame[1];
  const uint8_t *payload = frame.data() + 6;
  const size_t payload_len = frame.size() - 6;

  switch (type) {
    case A5_TYPE_STATUS:
      if (payload_len == 20) {
        this->handle_status_payload_(payload, payload_len);
      } else if (payload_len == 5) {
        this->handle_short_event_(payload, payload_len);
      } else {
        ESP_LOGD(TAG, "A5-02 unexpected length %u: %s", (unsigned) payload_len,
                 frame_to_hex(frame.data(), frame.size()).c_str());
      }
      break;
    case A5_TYPE_ACK:
      if (payload_len == 20) {
        // full status reply to the "01 84 40 00" request
        this->handle_status_payload_(payload, payload_len);
      } else {
        ESP_LOGV(TAG, "ACK: %s", frame_to_hex(frame.data(), frame.size()).c_str());
      }
      break;
    case A5_TYPE_NACK:
      ESP_LOGW(TAG, "Command rejected (NACK): %s", frame_to_hex(frame.data(), frame.size()).c_str());
      break;
    default:
      ESP_LOGD(TAG, "Unhandled frame type 0x%02X: %s", type, frame_to_hex(frame.data(), frame.size()).c_str());
      break;
  }
}

void LVClassic300SHumidifier::handle_status_payload_(const uint8_t *p, size_t len) {
  if (len < 20) return;

  // Every field below is published only when it differs from the last
  // known value, to avoid spamming HA on every 15s status poll. But that
  // comparison is against a member variable that starts at a compile-time
  // default (false / 0) -- so if a field's *actual first real reading*
  // happens to equal that default (e.g. water_empty=false, tank_removed=
  // false -- the normal, common case for both), it would never get its
  // first publish_state() call, and the entity would sit at "Unknown" in
  // HA indefinitely, until/unless that field ever actually changed.
  // `first_status` forces one publish per field on the very first
  // successfully parsed frame, regardless of whether it matches the
  // default, so every entity gets a real initial value.
  const bool first_status = !this->got_first_status_;
  this->got_first_status_ = true;

  const bool power = p[7] != 0;
  const bool tank_removed = p[8] != 0;
  const bool water_empty = p[9] != 0;
  const bool target_stop_active = p[10] != 0;
  const bool display_on = p[11] != 0;
  const bool mist_active = p[12] != 0;
  const uint8_t target_humidity = p[13];
  const uint8_t current_humidity = p[14];
  const int8_t temperature_c = static_cast<int8_t>(p[15]);
  const uint8_t mode_raw = p[16];
  const uint8_t level = p[17];
  const uint8_t night_light = p[18];
  const uint8_t error_code = p[19];

  const WorkMode mode = (mode_raw <= MODE_SLEEP) ? static_cast<WorkMode>(mode_raw) : MODE_UNKNOWN;

  if ((power != this->power_state_ || first_status) && this->power_binary_sensor_ != nullptr)
    this->power_binary_sensor_->publish_state(power);
  this->power_state_ = power;

  if ((tank_removed != this->tank_removed_ || first_status) && this->tank_removed_sensor_ != nullptr)
    this->tank_removed_sensor_->publish_state(tank_removed);
  this->tank_removed_ = tank_removed;

  if ((water_empty != this->water_empty_ || first_status) && this->water_empty_sensor_ != nullptr)
    this->water_empty_sensor_->publish_state(water_empty);
  this->water_empty_ = water_empty;

  if ((target_stop_active != this->target_stop_active_ || first_status) &&
      this->target_stop_active_sensor_ != nullptr)
    this->target_stop_active_sensor_->publish_state(target_stop_active);
  this->target_stop_active_ = target_stop_active;

  if ((display_on != this->display_state_ || first_status) && this->display_binary_sensor_ != nullptr)
    this->display_binary_sensor_->publish_state(display_on);
  this->display_state_ = display_on;

  if ((mist_active != this->mist_active_ || first_status) && this->mist_active_sensor_ != nullptr)
    this->mist_active_sensor_->publish_state(mist_active);
  this->mist_active_ = mist_active;

  if ((target_humidity != this->target_humidity_ || first_status) && this->target_humidity_sensor_ != nullptr)
    this->target_humidity_sensor_->publish_state(target_humidity);
  this->target_humidity_ = target_humidity;
  // keep the per-mode target caches in sync so a later switch back to a mode
  // re-sends the last known-good target instead of a stale default
  if (mode == MODE_AUTO) this->auto_target_ = target_humidity;
  if (mode == MODE_SLEEP) this->sleep_target_ = target_humidity;

  if ((current_humidity != this->current_humidity_ || first_status) && this->current_humidity_sensor_ != nullptr)
    this->current_humidity_sensor_->publish_state(current_humidity);
  this->current_humidity_ = current_humidity;

  if ((temperature_c != this->temperature_c_ || first_status) && this->temperature_sensor_ != nullptr)
    this->temperature_sensor_->publish_state(temperature_c);
  this->temperature_c_ = temperature_c;

  if ((mode != this->work_mode_ || first_status) && this->mode_text_sensor_ != nullptr) {
    const char *mode_str = mode == MODE_AUTO ? "auto" : mode == MODE_MANUAL ? "manual"
                            : mode == MODE_SLEEP                            ? "sleep"
                                                                             : "unknown";
    this->mode_text_sensor_->publish_state(mode_str);
  }
  this->work_mode_ = mode;

  if ((level != this->output_level_ || first_status) && this->output_level_sensor_ != nullptr)
    this->output_level_sensor_->publish_state(level);
  this->output_level_ = level;
  if (mode == MODE_MANUAL) this->manual_level_ = level;

  if ((night_light != this->night_light_level_ || first_status) && this->night_light_sensor_ != nullptr)
    this->night_light_sensor_->publish_state(night_light);
  this->night_light_level_ = night_light;

  if ((error_code != this->error_code_ || first_status) && this->error_text_sensor_ != nullptr) {
    std::string err;
    if (error_code == 0x00) {
      err = "ok";
    } else if (error_code == 0x01) {
      err = "E1 (tank sensor, candidate)";
    } else if (error_code == 0x02) {
      err = "E2 (unconfirmed)";
    } else {
      char buf[16];
      snprintf(buf, sizeof(buf), "unknown(0x%02X)", error_code);
      err = buf;
    }
    this->error_text_sensor_->publish_state(err);
  }
  this->error_code_ = error_code;

  if (this->last_status_frame_text_sensor_ != nullptr)
    this->last_status_frame_text_sensor_->publish_state(frame_to_hex(p, len));

  this->status_callback_.call();
}

void LVClassic300SHumidifier::handle_short_event_(const uint8_t *p, size_t len) {
  // observed form: 01 XX D1 00 YY  (power-button hold/release events)
  if (len < 5) return;
  ESP_LOGD(TAG, "Short MCU event: %s", frame_to_hex(p, len).c_str());

  if (p[1] == 0x02 && p[4] == 0x02) {
    ESP_LOGI(TAG, "Power button hold ~5s detected");
    this->power_hold_5s_trigger_.trigger();
  } else if (p[1] == 0x02 && p[4] == 0x03) {
    ESP_LOGI(TAG, "Power button released after ~5s hold");
  } else if (p[1] == 0x03 && p[4] == 0x04) {
    ESP_LOGI(TAG, "Power button hold ~15s detected");
    this->power_hold_15s_trigger_.trigger();
  }
}

void LVClassic300SHumidifier::send_command_(const uint8_t *payload, size_t len) {
  uint8_t frame[64];
  if (len + 6 > sizeof(frame)) {
    ESP_LOGE(TAG, "Command payload too long (%u bytes)", (unsigned) len);
    return;
  }

  frame[0] = 0xA5;
  frame[1] = A5_TYPE_CMD;
  frame[2] = this->next_packet_id_++;
  frame[3] = static_cast<uint8_t>(len & 0xFF);
  frame[4] = static_cast<uint8_t>((len >> 8) & 0xFF);
  frame[5] = 0x00;  // checksum placeholder

  memcpy(&frame[6], payload, len);

  const size_t total_len = len + 6;
  frame[5] = a5_checksum(frame, total_len);

  this->write_array(frame, total_len);
}

void LVClassic300SHumidifier::request_status() {
  const uint8_t payload[] = {0x01, 0x84, 0x40, 0x00};
  this->send_command_(payload, sizeof(payload));
}

void LVClassic300SHumidifier::set_power(bool on) {
  const uint8_t payload[] = {0x01, 0x00, 0xA0, 0x00, static_cast<uint8_t>(on ? 0x01 : 0x00)};
  this->send_command_(payload, sizeof(payload));
}

void LVClassic300SHumidifier::set_display(bool on) {
  const uint8_t payload[] = {0x01, 0x05, 0xA1, 0x00, static_cast<uint8_t>(on ? 0x64 : 0x00)};
  this->send_command_(payload, sizeof(payload));
}

void LVClassic300SHumidifier::set_night_light(uint8_t percent) {
  if (percent > 100) percent = 100;
  const uint8_t payload[] = {0x01, 0x03, 0xA0, 0x00, 0x01, percent};
  this->send_command_(payload, sizeof(payload));
}

void LVClassic300SHumidifier::set_stop_at_target(bool on) {
  const uint8_t payload[] = {0x01, 0xE5, 0xA5, 0x00, static_cast<uint8_t>(on ? 0x01 : 0x00)};
  this->send_command_(payload, sizeof(payload));
  // not reported back in the status payload (see docs/A5_UART_protocol.md),
  // so this is the only place this bit of state is tracked
  this->stop_at_target_state_ = on;
}

void LVClassic300SHumidifier::send_mode_sync_preamble() {
  const uint8_t payload[] = {0x01, 0x29, 0xA1, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00};
  this->send_command_(payload, sizeof(payload));
}

void LVClassic300SHumidifier::set_manual_level(uint8_t level) {
  if (level < 1) level = 1;
  if (level > 9) level = 9;
  const uint8_t payload[] = {0x01, 0x60, 0xA2, 0x00, 0x00, 0x01, level};
  this->send_command_(payload, sizeof(payload));
  this->manual_level_ = level;
}

void LVClassic300SHumidifier::set_auto_target(uint8_t target_percent) {
  const uint8_t low = target_percent > 5 ? target_percent - 5 : 0;
  const uint8_t high = target_percent <= 95 ? target_percent + 5 : 100;
  const uint8_t payload[] = {0x01, 0x80, 0x40, 0x00, target_percent, low, high, 0x09, 0x05, 0x01};
  this->send_command_(payload, sizeof(payload));
  this->auto_target_ = target_percent;
}

void LVClassic300SHumidifier::set_sleep_target(uint8_t target_percent) {
  const uint8_t low = target_percent > 5 ? target_percent - 5 : 0;
  const uint8_t high = target_percent <= 95 ? target_percent + 5 : 100;
  const uint8_t payload[] = {0x01, 0x82, 0x40, 0x00, target_percent, low, high, 0x09, 0x05, 0x01};
  this->send_command_(payload, sizeof(payload));
  this->sleep_target_ = target_percent;
}

void LVClassic300SHumidifier::set_mode(WorkMode mode, uint8_t target_or_level) {
  this->send_mode_sync_preamble();
  switch (mode) {
    case MODE_MANUAL:
      this->set_manual_level(target_or_level);
      break;
    case MODE_AUTO:
      this->set_auto_target(target_or_level);
      break;
    case MODE_SLEEP:
      this->set_sleep_target(target_or_level);
      break;
    default:
      break;
  }
  this->work_mode_ = mode;
}

}  // namespace lv_classic300s_humidifier
}  // namespace esphome
